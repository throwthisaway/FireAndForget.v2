#include "pch.h"
#include "Renderer.h"
#include "StringUtil.h"
#include "..\Common\DirectXHelper.h"
#include "SIMDTypeAliases.h"
#include "..\source\cpp\VertexTypes.h"
#include "..\source\cpp\DeferredBindings.h"
#include "..\..\source\cpp\BufferUtils.h"
#include "..\..\source\cpp\ShaderStructures.h"
#include <glm/gtc/matrix_transform.hpp>
// TODO::
// - irradiance is flipped
// - requesting new ppol in cbframealloc causes "CPU descriptor handle expected, but specified handle refers to a GPU descriptor handle." during CreateCBV
// - all embedded rootsignatures are read from the ps source
// - depthstencil transition to depth write happens at the present resource barrier at the very end
// - DownsampleDepth transitions depth stencil to pixel shader resouce
// - remove deviceResource commandallocators
// - remove rtvs[framecount]
// - swap offset and frame in createsrv
// - fix rootparamindex counting via templates (1 root paramindex for vs, 1 for ps instead of buffercount)
// - rename: shaders “scene.vs.hlsl” and “scene.ps.hlsl”, I create a third file “scene.rs.hlsli”, which contains two things:
// - simplify RootSignature access for SetGraphicsRootSignature
using namespace ShaderStructures;
// Resource Binding Flow of Control - https://msdn.microsoft.com/en-us/library/windows/desktop/mt709154(v=vs.85).aspx
// Resource Binding - https://msdn.microsoft.com/en-us/library/windows/desktop/dn899206(v=vs.85).aspx
// Creating Descriptors - https://msdn.microsoft.com/en-us/library/windows/desktop/dn859358(v=vs.85).aspx
// https://www.braynzarsoft.net/viewtutorial/q16390-04-directx-12-braynzar-soft-tutorials

static const float Black[] = { 0.f, 0.f, 0.f, 0.f };

using namespace Microsoft::WRL;
using namespace DirectX;
using namespace Windows::Foundation;

namespace {
	static constexpr size_t defaultCBFrameAllocSize = 16384, defaultDescFrameAllocCount = 32,
		defaultRTVDescCount = 16;
	inline uint32_t NumMips(uint32_t w, uint32_t h) {
        uint32_t res;
        _BitScanReverse((unsigned long*)&res, w | h);
        return res + 1;
    }
};
Renderer::Renderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	pipelineStates_(deviceResources.get()),
	m_deviceResources(deviceResources) {

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

Renderer::~Renderer() {}

void Renderer::SaveState() {}
void Renderer::BeginUploadResources() {
	DX::ThrowIfFailed(bufferUpload_.cmdAllocator->Reset());
	DX::ThrowIfFailed(bufferUpload_.cmdList->Reset(bufferUpload_.cmdAllocator.Get(), nullptr));
}
void Renderer::EndUploadResources() {
	DX::ThrowIfFailed(bufferUpload_.cmdList->Close());
	ID3D12CommandList* ppCommandLists[] = { bufferUpload_.cmdList.Get() };
	
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	m_deviceResources->WaitForGpu();
	// cleanup
	bufferUpload_.intermediateResources.clear();
}
namespace {
	DXGI_FORMAT PixelFormatToDXGIFormat(Img::PixelFormat format) {
		switch (format) {
			case Img::PixelFormat::RGBA8:
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			case Img::PixelFormat::BGRA8:
				return DXGI_FORMAT_B8G8R8A8_UNORM;
			case Img::PixelFormat::Greyscale8:
				return DXGI_FORMAT_R8_UNORM;
			case Img::PixelFormat::RGBAF16:
				return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case Img::PixelFormat::RGBAF32:
				return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case Img::PixelFormat::RGB8:
			case Img::PixelFormat::BGR8:
			default:
				assert(false);
		}
		return DXGI_FORMAT_UNKNOWN;
	}
}
uint32_t Renderer::GetCurrenFrameIndex() const {
	return m_deviceResources->GetCurrentFrameIndex();
}
Dim Renderer::GetDimensions(TextureIndex index) {
	D3D12_RESOURCE_DESC desc = buffers_[index].resource->GetDesc();
	return { (decltype(Dim::w))desc.Width, (decltype(Dim::h))desc.Height };
}
TextureIndex Renderer::CreateTexture(const void* buffer, uint64_t width, uint32_t height, Img::PixelFormat format, LPCSTR label) {
	DXGI_FORMAT dxgiFmt = PixelFormatToDXGIFormat(format);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(dxgiFmt, width, height, 1, 1);

	auto device = m_deviceResources->GetD3DDevice();
	Microsoft::WRL::ComPtr<ID3D12Resource> resource, bufferUpload;

	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	DX::ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)));

	UINT64 textureUploadBufferSize;
	device->GetCopyableFootprints(&bufferDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	DX::ThrowIfFailed(device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&bufferUpload)));
	bufferUpload_.intermediateResources.push_back(bufferUpload);
	if (label) DX::SetName(resource.Get(), s2ws(label).c_str());
	{
		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = buffer;
		data.RowPitch = width * Img::BytesPerPixel(format);
		data.SlicePitch = data.RowPitch * height;

		UpdateSubresources(bufferUpload_.cmdList.Get(), resource.Get(), bufferUpload.Get(), 0, 0, 1, &data);

		CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		bufferUpload_.cmdList->ResourceBarrier(1, &vertexBufferResourceBarrier);
	}
	buffers_.push_back({ resource, 0, 0, dxgiFmt });
	return (TextureIndex)buffers_.size() - 1;
}

BufferIndex Renderer::CreateBuffer(const void* buffer, size_t sizeInBytes) {
	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);

	auto d3dDevice = m_deviceResources->GetD3DDevice();
	Microsoft::WRL::ComPtr<ID3D12Resource> resource, bufferUpload;

	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)));

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&bufferUpload)));
	bufferUpload_.intermediateResources.push_back(bufferUpload);
	NAME_D3D12_OBJECT(resource);
	{
		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = buffer;
		data.RowPitch = sizeInBytes;
		data.SlicePitch = data.RowPitch;

		UpdateSubresources(bufferUpload_.cmdList.Get(), resource.Get(), bufferUpload.Get(), 0, 0, 1, &data);

		CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		bufferUpload_.cmdList->ResourceBarrier(1, &vertexBufferResourceBarrier);
	}
	buffers_.push_back({ resource, resource->GetGPUVirtualAddress(), sizeInBytes});
	return (BufferIndex)buffers_.size() - 1;
}

void Renderer::CreateDeviceDependentResources() {
#ifdef DXGI_ANALYSIS
	HRESULT hr = DXGIGetDebugInterface1(0, __uuidof(pGraphicsAnalysis), reinterpret_cast<void**>(pGraphicsAnalysis.GetAddressOf()));
	graphicsDebugging = SUCCEEDED(hr);
#endif
	auto device = m_deviceResources->GetD3DDevice();
	for (int i = 0; i < _countof(frames_); ++i) {
		frames_[i].cb.Init(device, defaultCBFrameAllocSize);
		frames_[i].desc.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, defaultDescFrameAllocCount, true);
		DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frames_[i].deferredCommandAllocator)));
	}
	DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames_[0].deferredCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&deferredCommandList_)));
	deferredCommandList_->Close();
	NAME_D3D12_OBJECT(deferredCommandList_);

	pipelineStates_.CreateDeviceDependentResources();

	// Buffer upload...
	DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&bufferUpload_.cmdAllocator)));
	DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, bufferUpload_.cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&bufferUpload_.cmdList)));
	bufferUpload_.cmdList->Close();
	NAME_D3D12_OBJECT(bufferUpload_.cmdList);
	// Buffer upload...

	pipelineStates_.completionTask_.then([this]() {
		loadingComplete_ = true;
	});

	//prepass
	DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&prePass_.cmdAllocator)));
	DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, prePass_.cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&prePass_.cmdList)));
	prePass_.cmdList->Close();
	//DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&prePass_.computeCmdAllocator)));
	//DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, prePass_.computeCmdAllocator.Get(), nullptr, IID_PPV_ARGS(&prePass_.computeCmdList)));
	//prePass_.computeCmdList->Close();

	for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
		ID3D12CommandAllocator* firstCommandAllocator = nullptr;
		for (UINT n = 0; n < FrameCount; n++) {
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
			DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
			commandAllocators_.push_back(commandAllocator);
			if (!n) firstCommandAllocator = commandAllocator.Get();
		}
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, firstCommandAllocator, nullptr, IID_PPV_ARGS(&commandList)));
		commandList->Close();
		//NAME_D3D12_OBJECT(commandList);
		commandLists_.push_back(commandList);
	}

	BeginUploadResources();
	{
		// fullscreen quad...
		VertexFSQuad quad[] = { { { -1., -1.f },{ 0.f, 1.f } },{ { -1., 1.f },{ 0.f, 0.f } },{ { 1., -1.f },{ 1.f, 1.f } },{ { 1., 1.f },{ 1.f, 0.f } } };
		fsQuad_ = CreateBuffer(quad, sizeof(quad));
	}
	EndUploadResources();
}
void Renderer::BeginPrePass() {
#ifdef DXGI_ANALYSIS
	if (graphicsDebugging) pGraphicsAnalysis->BeginCapture();
#endif
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(prePass_.cmdAllocator->Reset());
	DX::ThrowIfFailed(prePass_.cmdList->Reset(prePass_.cmdAllocator.Get(), nullptr));
	//DX::ThrowIfFailed(prePass_.computeCmdAllocator->Reset());
	//DX::ThrowIfFailed(prePass_.computeCmdList->Reset(prePass_.computeCmdAllocator.Get(), nullptr));
	prePass_.cb.Init(device, defaultCBFrameAllocSize);
	prePass_.desc.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, defaultDescFrameAllocCount, true);
	prePass_.rtv.desc.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, defaultDescFrameAllocCount, false);
}
TextureIndex Renderer::GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint32_t dim, ShaderId shader, bool mip, LPCSTR label) {
	assert(vb != InvalidBuffer);
	assert(ib != InvalidBuffer);
	auto device = m_deviceResources->GetD3DDevice();
	DXGI_FORMAT fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const int count = 6;
	ComPtr<ID3D12Resource> resource;
	{
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(fmt, dim, dim, count, (mip) ? UINT16(NumMips(dim, dim)) : 1);
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		if (mip) desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		DX::ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			nullptr,
			IID_PPV_ARGS(&resource)));
			if (label) DX::SetName(resource.Get(), s2ws(label).c_str());
	}
	TextureIndex res = (TextureIndex)buffers_.size();
	buffers_.push_back({ resource, 0, 0, fmt });

	ID3D12GraphicsCommandList* commandList = prePass_.cmdList.Get();
	PIXBeginEvent(commandList, 0, L"GenCubeMap"); 
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	const auto rtvDescSize = prePass_.rtv.desc.GetDescriptorSize();
	{
		auto entry = prePass_.rtv.desc.Push(count);
		rtvHandle = entry.cpuHandle;
		auto handle = rtvHandle;
		for (int i = 0; i < count; ++i) {
			D3D12_RENDER_TARGET_VIEW_DESC desc = {};
			desc.Format = fmt;
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = 1;
			desc.Texture2DArray.FirstArraySlice = i;
			device->CreateRenderTargetView(resource.Get(), &desc, handle);
			handle.Offset(rtvDescSize);
		}
	}
	D3D12_VIEWPORT viewport = { 0, 0, FLOAT(dim), FLOAT(dim) };
	D3D12_RECT scissor = { 0, 0, (LONG)dim, (LONG)dim };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
	auto& state = pipelineStates_.states_[shader];
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	commandList->SetPipelineState(state.pipelineState.Get());
	const auto descSize = prePass_.desc.GetDescriptorSize();
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvGPUHandle, srvGPUHandle;
	auto entry = prePass_.desc.Push(count + 1);
	cbvGPUHandle = entry.gpuHandle;	// for vs cbv binding
	srvGPUHandle = entry.gpuHandle; srvGPUHandle.Offset(count * descSize); // for ps srv binding
	// cube view matrices for cube env. map generation
	const glm::vec3 at[] = {/*+x*/{1.f, 0.f, 0.f},/*-x*/{-1.f, 0.f, 0.f},/*+y*/{0.f, 1.f, 0.f},/*-y*/{0.f, -1.f, 0.f},/*+z*/{0.f, 0.f, 1.f},/*-z*/{0.f, 0.f, -1.f} },
		up[] = { {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f} };
	const int inc = AlignTo<int, 256>(sizeof(glm::mat4x4)), size = inc * count;
	auto cb = prePass_.cb.Alloc(size);
	uint8_t* p = cb.cpuAddress;
	auto handle = entry.cpuHandle;
	auto gpuAddress = cb.gpuAddress;
	for (int i = 0; i < count; ++i, p += inc) {
		glm::mat4x4 v = glm::lookAtLH({ 0.f, 0.f, 0.f }, at[i], up[i]);
		memcpy(p, &v, sizeof(v));
		prePass_.desc.CreateCBV(handle, gpuAddress, inc);
		handle.Offset(descSize); gpuAddress += inc;
	}

	if (buffers_[tex].resource->GetDesc().DepthOrArraySize > 1)
		prePass_.desc.CreateCubeSRV(handle, buffers_[tex].resource.Get());
	else
		prePass_.desc.CreateSRV(handle, buffers_[tex].resource.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews = { buffers_[vb].bufferLocation + submesh.vbByteOffset,
			(UINT)buffers_[vb].size, submesh.stride } ;
	commandList->IASetVertexBuffers(0, 1, &vertexBufferViews);
	D3D12_INDEX_BUFFER_VIEW	indexBufferView = {
			buffers_[ib].bufferLocation + submesh.ibByteOffset,
			(UINT)buffers_[ib].size,
			DXGI_FORMAT_R16_UINT };
	commandList->IASetIndexBuffer(&indexBufferView);
	commandList->SetGraphicsRootDescriptorTable(1, srvGPUHandle);
	for (int i = 0; i < count; ++i) {
		commandList->SetGraphicsRootDescriptorTable(0, cbvGPUHandle);
		cbvGPUHandle.Offset(descSize);
		commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
		rtvHandle.Offset(rtvDescSize);
		commandList->DrawIndexedInstanced(submesh.count, 1, 0, 0, 0);
	}
	if (mip) {
		GenMips(resource, fmt, dim, dim, count);
	} else {
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
	}
	PIXEndEvent(commandList);
	return res;
}

void Renderer::GenMips(Microsoft::WRL::ComPtr<ID3D12Resource> resource, DXGI_FORMAT fmt, int width, int height, uint32_t arraySize) {
	const uint32_t kMaxMipsPerCall = 4, kNumThreads = 8;
	ID3D12GraphicsCommandList* commandList = prePass_.cmdList.Get();
	PIXBeginEvent(commandList, 0, L"GenMips");
	
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
	enum class DescEntries{ kCBuffer, kSrcTexture, kDstUAV0, kDstUAV1, kDstUAV2, kDstUAV3, kCount};
	const auto numMips = NumMips(width, height) - 1;
	for (uint32_t arrayIndex = 0; arrayIndex < arraySize; ++arrayIndex) {
		for (uint32_t i = 0; i < numMips;) {
			int w = width >> i, h = height >> i;
			auto shaderId = ShaderStructures::GenMips;
			if (w & 1 && h & 1) shaderId = ShaderStructures::GenMipsOddXOddY;
			else if (w & 1) shaderId = ShaderStructures::GenMipsOddX;
			else if (h & 1) shaderId = ShaderStructures::GenMipsOddY;
			auto& state = pipelineStates_.states_[shaderId];
			commandList->SetPipelineState(state.pipelineState.Get());
			w >>= 1; h >>= 1;
			w = std::max(1, w); h = std::max(1, h);
			struct alignas(16) {
				uint32_t srcMipLevel;
				uint32_t numMipLevels;
				float2 texelSize; // 1. / dim
			}cbuffer;
			_BitScanForward((DWORD*)& cbuffer.numMipLevels, w | h);
			cbuffer.srcMipLevel = i;
			cbuffer.texelSize = 1.f / float2{ w, h };
			if (cbuffer.numMipLevels > kMaxMipsPerCall - 1) {
				cbuffer.numMipLevels = kMaxMipsPerCall;
			} else {
				cbuffer.numMipLevels = 1 + cbuffer.numMipLevels;
			}
			if (cbuffer.numMipLevels + cbuffer.srcMipLevel > numMips) cbuffer.numMipLevels = numMips - cbuffer.srcMipLevel;
			auto cb = prePass_.cb.Alloc(sizeof(cbuffer));
			memcpy(cb.cpuAddress, &cbuffer, sizeof(cbuffer));
			auto entry = prePass_.desc.Push(int(DescEntries::kCount));
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(entry.cpuHandle);
			auto descSize = prePass_.desc.GetDescriptorSize();
			prePass_.desc.CreateCBV(handle, cb.gpuAddress, AlignTo<uint32_t, 256>(sizeof(cbuffer)));
			handle.Offset(descSize);
			if (arraySize)
				prePass_.desc.CreateSRV(handle, resource.Get(), arrayIndex);
			else
				prePass_.desc.CreateSRV(handle, resource.Get());
			handle.Offset(descSize);
			for (uint32_t j = 0; j < cbuffer.numMipLevels; ++j) {
				if (arraySize)
					prePass_.desc.CreateArrayUAV(handle, resource.Get(), arrayIndex, j + i + 1);
				else
					prePass_.desc.CreateUAV(handle, resource.Get(), j + i + 1);
				handle.Offset(descSize);
			}
			for (int j = cbuffer.numMipLevels; j < kMaxMipsPerCall; ++j) {
				prePass_.desc.CreateUAV(handle, nullptr);
				handle.Offset(descSize);
			}
			ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(entry.gpuHandle);
			commandList->SetComputeRootSignature(state.rootSignature.Get());
			commandList->SetComputeRootDescriptorTable(0, gpuHandle);
			gpuHandle.Offset(descSize);
			commandList->SetComputeRootDescriptorTable(1, gpuHandle);
			gpuHandle.Offset(descSize);
			commandList->SetComputeRootDescriptorTable(2, gpuHandle);
			//auto d = AlignTo<int, kNumThreads>(w) / kNumThreads;
			commandList->Dispatch(AlignTo<int, kNumThreads>(w) / kNumThreads, AlignTo<int, kNumThreads>(h) / kNumThreads, 1);
			// TODO::???
			/*if (DstWidth == 0)
				DstWidth = 1;
			if (DstHeight == 0)
				DstHeight = 1;
			Context.Dispatch2D(DstWidth, DstHeight);*/
			i += cbuffer.numMipLevels;
			{
				// Otherwise the mip levels become glitchy between loop iteration
				// Insert UAV barrier
				D3D12_RESOURCE_BARRIER desc = {};
				desc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				desc.UAV.pResource = resource.Get();
				commandList->ResourceBarrier(1, &desc);
			}
		}
	}
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
	// TODO:: 	???		Context.InsertUAVBarrier(*this);
	PIXEndEvent(commandList);
}
TextureIndex Renderer::GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint32_t dim, ShaderId shader, LPCSTR label) {
	assert(vb != InvalidBuffer);
	assert(ib != InvalidBuffer);
	auto device = m_deviceResources->GetD3DDevice();
	DXGI_FORMAT fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const int psBindingCount = 3;
	const int faceCount = 6/*face count*/, mipLevelCount = 5, count = faceCount + (mipLevelCount * psBindingCount);
	ComPtr<ID3D12Resource> resource;
	{
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(fmt, dim, dim, faceCount, mipLevelCount);
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		DX::ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			nullptr,
			IID_PPV_ARGS(&resource)));
			if (label) DX::SetName(resource.Get(), s2ws(label).c_str());
	}
	TextureIndex res = (TextureIndex)buffers_.size();
	buffers_.push_back({ resource, 0, 0, fmt });

	ID3D12GraphicsCommandList* commandList = prePass_.cmdList.Get();
	PIXBeginEvent(commandList, 0, L"GenPrefilteredEnvCubeMap");
	auto& state = pipelineStates_.states_[shader];
	commandList->SetPipelineState(state.pipelineState.Get());
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	D3D12_VIEWPORT viewport = { 0, 0, FLOAT(dim), FLOAT(dim) };
	D3D12_RECT scissor = { 0, 0, (LONG)dim, (LONG)dim };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
	const auto descSize = prePass_.desc.GetDescriptorSize();
	auto entry = prePass_.desc.Push(count);
	CD3DX12_GPU_DESCRIPTOR_HANDLE vsGPUHandle = entry.gpuHandle;	// for vs cbv binding
	// cube view matrices for cube env. map generation
	const glm::vec3 at[] = {/*+x*/{1.f, 0.f, 0.f},/*-x*/{-1.f, 0.f, 0.f},/*+y*/{0.f, 1.f, 0.f},/*-y*/{0.f, -1.f, 0.f},/*+z*/{0.f, 0.f, 1.f},/*-z*/{0.f, 0.f, -1.f} },
		up[] = { {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f} };
	const int inc = AlignTo<int, 256>(sizeof(glm::mat4x4)), vsSize = inc * faceCount;
	auto cb = prePass_.cb.Alloc(vsSize);
	uint8_t* p = cb.cpuAddress;
	auto handle = entry.cpuHandle;
	auto gpuAddress = cb.gpuAddress;
	for (int i = 0; i < faceCount; ++i, p += inc) {
		glm::mat4x4 v = glm::lookAtLH({ 0.f, 0.f, 0.f }, at[i], up[i]);
		memcpy(p, &v, sizeof(v));
		prePass_.desc.CreateCBV(handle, gpuAddress, inc);
		handle.Offset(descSize); gpuAddress += inc;
	}
	
	CD3DX12_GPU_DESCRIPTOR_HANDLE psGPUHandle = entry.gpuHandle; psGPUHandle.Offset(faceCount * descSize); // for ps bindings
	handle = entry.cpuHandle; handle.Offset(faceCount * descSize);
	// roughness
	auto cb0 = prePass_.cb.Alloc(AlignTo<int, 256>(sizeof(float)) * mipLevelCount);
	const int inc0 = AlignTo<int, 256>(sizeof(float));
	float* p0 = (float*)cb0.cpuAddress;
	gpuAddress = cb0.gpuAddress;

	// resolution
	auto cb1 = prePass_.cb.Alloc(sizeof(float));
	const int inc1 = AlignTo<int, 256>(sizeof(float));
	float* p1 = (float*)cb1.cpuAddress;
	*p1 = (float)dim;

	for (int i = 0; i < mipLevelCount; ++i, p0 += inc0 / sizeof(*p0)) {
		*p0 = (float) i / (mipLevelCount - 1);
		prePass_.desc.CreateCBV(handle, gpuAddress, inc0);
		handle.Offset(descSize); gpuAddress += inc0;	// only the roughness changes per drawcall

		// set the same cb for all the handles
		prePass_.desc.CreateCBV(handle, cb1.gpuAddress, inc1);
		handle.Offset(descSize);

		// set the same texture for all the handles
		prePass_.desc.CreateCubeSRV(handle, buffers_[tex].resource.Get());
		handle.Offset(descSize);
	}

	// set up rtv for all the cube faces
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	const auto rtvDescSize = prePass_.rtv.desc.GetDescriptorSize();
	{
		auto entry = prePass_.rtv.desc.Push(faceCount * mipLevelCount);
		rtvHandle = entry.cpuHandle;
		auto handle = rtvHandle;
		for (int i = 0; i < faceCount * mipLevelCount; ++i) {
			D3D12_RENDER_TARGET_VIEW_DESC desc = {};
			desc.Format = fmt;
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = 1;
			desc.Texture2DArray.FirstArraySlice = i % faceCount;
			desc.Texture2DArray.MipSlice = i / faceCount;
			device->CreateRenderTargetView(resource.Get(), &desc, handle);
			handle.Offset(rtvDescSize);
		}
	}

	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews = { buffers_[vb].bufferLocation + submesh.vbByteOffset,
			(UINT)buffers_[vb].size, submesh.stride } ;
	commandList->IASetVertexBuffers(0, 1, &vertexBufferViews);
	D3D12_INDEX_BUFFER_VIEW	indexBufferView = {
			buffers_[ib].bufferLocation + submesh.ibByteOffset,
			(UINT)buffers_[ib].size,
			DXGI_FORMAT_R16_UINT };
	commandList->IASetIndexBuffer(&indexBufferView);
	for (int j = 0; j < mipLevelCount; ++j) {
		D3D12_VIEWPORT viewport = { 0, 0, FLOAT(dim), FLOAT(dim) };
		D3D12_RECT scissor = { 0, 0, (LONG)dim, (LONG)dim };
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissor);
		dim >>= 1;
		commandList->SetGraphicsRootDescriptorTable(1, psGPUHandle);
		psGPUHandle.Offset(descSize * psBindingCount);
		auto tempVSGPUHandle = vsGPUHandle;
		for (int i = 0; i < faceCount; ++i) {
			commandList->SetGraphicsRootDescriptorTable(0, tempVSGPUHandle);
			tempVSGPUHandle.Offset(descSize);
			commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
			rtvHandle.Offset(rtvDescSize);
			commandList->DrawIndexedInstanced(submesh.count, 1, 0, 0, 0);
		}
	}
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
	PIXEndEvent(commandList);
	return res;
}
TextureIndex Renderer::GenBRDFLUT(uint32_t dim, ShaderId shader, LPCSTR label) {
	auto device = m_deviceResources->GetD3DDevice();
	DXGI_FORMAT fmt = DXGI_FORMAT_R16G16_FLOAT;
	ComPtr<ID3D12Resource> resource;
	{
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(fmt, dim, dim, 1, 1);
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		DX::ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			nullptr,
			IID_PPV_ARGS(&resource)));
		if (label) DX::SetName(resource.Get(), s2ws(label).c_str());
	}
	TextureIndex res = (TextureIndex)buffers_.size();
	buffers_.push_back({ resource, 0, 0, fmt });

	auto entry = prePass_.rtv.desc.Push(1);
	D3D12_RENDER_TARGET_VIEW_DESC desc = {};
	desc.Format = fmt;
	desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	device->CreateRenderTargetView(resource.Get(), &desc, entry.cpuHandle);

	ID3D12GraphicsCommandList* commandList = prePass_.cmdList.Get();
	PIXBeginEvent(commandList, 0, L"GenBRDFLUT");
	auto& state = pipelineStates_.states_[shader];
	commandList->SetPipelineState(state.pipelineState.Get());
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	D3D12_VIEWPORT viewport = { 0, 0, FLOAT(dim), FLOAT(dim) };
	D3D12_RECT scissor = { 0, 0, (LONG)dim, (LONG)dim };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
	commandList->OMSetRenderTargets(1, &entry.cpuHandle, false, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	assert(fsQuad_ != InvalidBuffer);
	
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
		{ buffers_[fsQuad_].bufferLocation,
		(UINT)buffers_[fsQuad_].size,
		(UINT)sizeof(VertexFSQuad) } };

	commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
	commandList->DrawInstanced(4, 1, 0, 0);
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
	PIXEndEvent(commandList);
	return res;
}
void Renderer::EndPrePass() {
	// TODO:: fills the entire constant buffer !!!!
	//frames_[0].cb.Fill(0x7f);
	//frames_[1].cb.Fill(0x7f);
	//frames_[2].cb.Fill(0x7f);
	DX::ThrowIfFailed(prePass_.cmdList->Close());
	//DX::ThrowIfFailed(prePass_.computeCmdList->Close());
	ID3D12CommandList* ppCommandLists[] = { prePass_.cmdList.Get()/*, prePass_.computeCmdList.Get()*/};
	
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	// TODO:: fills the first 16k only constant buffer !!!!
	/*frames_[0].cb.Fill(0x6f);
	frames_[1].cb.Fill(0x6f);
	frames_[2].cb.Fill(0x6f);*/
	m_deviceResources->WaitForGpu();
	/*frames_[0].cb.Fill(0x5f);
	frames_[1].cb.Fill(0x5f);
	frames_[2].cb.Fill(0x5f);*/
#ifdef DXGI_ANALYSIS
	if (graphicsDebugging) pGraphicsAnalysis->EndCapture();
#endif

	//cleanup
	prePass_.rtv.desc = {};
	prePass_.cb = {};
	prePass_.desc = {};
}
void Renderer::CreateWindowSizeDependentResources() {
	auto device = m_deviceResources->GetD3DDevice();
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	scissorRect_ = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
	rtv_.desc.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, defaultRTVDescCount, false);
	rtv_.entry = rtv_.desc.Push(_countof(PipelineStates::deferredRTFmts));
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle = rtv_.entry.cpuHandle;
	D3D12_CLEAR_VALUE clearValue;
	memcpy(clearValue.Color, Black, sizeof(clearValue.Color));
	for (int j = 0; j < _countof(PipelineStates::deferredRTFmts); ++j) {
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(PipelineStates::deferredRTFmts[j], (UINT)viewport.Width, (UINT)viewport.Height, 1, 1);
		clearValue.Format = PipelineStates::deferredRTFmts[j];
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		DX::ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,// StartRenderPass sets up the proper state D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clearValue,
			IID_PPV_ARGS(&resource)));
		rtt_[j] = resource;
		WCHAR name[25];
		if (swprintf_s(name, L"renderTarget[%u]", j) > 0) DX::SetName(resource.Get(), name);

		{
			D3D12_RENDER_TARGET_VIEW_DESC desc = {};
			desc.Format = PipelineStates::deferredRTFmts[j];
			desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			device->CreateRenderTargetView(rtt_[j].Get(), &desc, handle);
			handle.Offset(rtv_.desc.GetDescriptorSize());
		}
	}

	// half-res depth
	DXGI_FORMAT halfResDepthFmt = DXGI_FORMAT_R32_FLOAT;
	CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(halfResDepthFmt, ((UINT)viewport.Width) >> 1, ((UINT)viewport.Height) >> 1, 1, 1);
	desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	DX::ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&halfResDepth_)));
	NAME_D3D12_OBJECT(halfResDepth_);
	{
		rtv_.halfResDepthEntry = rtv_.desc.Push(1);
		D3D12_RENDER_TARGET_VIEW_DESC desc = {};
		desc.Format = halfResDepthFmt;
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		device->CreateRenderTargetView(halfResDepth_.Get(), &desc, handle);
		handle.Offset(rtv_.desc.GetDescriptorSize());
	}
}

void Renderer::Update(DX::StepTimer const& timer) {
	if (loadingComplete_){
	}
}

void Renderer::BeginRender() {
	if (!loadingComplete_) return;

	auto currentFrameIndex = m_deviceResources->GetCurrentFrameIndex();
	frame_ = &frames_[currentFrameIndex];
	frame_->cb.Reset();
	frame_->desc.Reset();
	// TODO:: is this used???
	m_deviceResources->GetCommandAllocator()->Reset();
	size_t i = 0;
	for (auto& commandList : commandLists_) {
		auto* commandAllocator = commandAllocators_[m_deviceResources->GetCurrentFrameIndex() + i * FrameCount].Get();
		DX::ThrowIfFailed(commandAllocator->Reset());
		DX::ThrowIfFailed(commandList->Reset(commandAllocator, nullptr));
		++i;
	}
	DX::ThrowIfFailed(frame_->deferredCommandAllocator->Reset());
	DX::ThrowIfFailed(deferredCommandList_->Reset(frame_->deferredCommandAllocator.Get(), nullptr));
	// Initialise depth
	// TODO:: !!! this commandlist might not be used everytime
	auto* commandList = commandLists_[0].Get();
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect_);
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_deviceResources->GetDepthStencilView();
	commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void Renderer::StartForwardPass() {
	if (!loadingComplete_) return;
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	bool first = true;
	for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
		auto& state = pipelineStates_.states_[i];
		if (state.pass != PipelineStates::State::RenderPass::Forward) continue;
		auto* commandList = commandLists_[i].Get();
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect_);
		if (first) {
			first = false;
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			commandList->ResourceBarrier(1, &barrier);
			commandList->ClearRenderTargetView(m_deviceResources->GetRenderTargetView(), Black, 0, nullptr);
		}
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_deviceResources->GetDepthStencilView();
		commandList->OMSetRenderTargets(1, &m_deviceResources->GetRenderTargetView(), false, &depthStencilView);
		commandList->SetPipelineState(state.pipelineState.Get());
		commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	}
}

void Renderer::StartGeometryPass() {
	if (!loadingComplete_) return;
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();

	// assign pipelinestates with command lists
	bool first = true;
	for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
		auto& state = pipelineStates_.states_[i];
		if (state.pass != PipelineStates::State::RenderPass::Geometry) continue;
		auto* commandList = commandLists_[i].Get();
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect_);
		if (first) {
			first = false;
			CD3DX12_RESOURCE_BARRIER barriers[_countof(PipelineStates::deferredRTFmts)];
			for (int i = 0; i < _countof(barriers); ++i) {
				barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(rtt_[i].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
			}
			commandList->ResourceBarrier(_countof(barriers), barriers);
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(rtv_.entry.cpuHandle);
			// TODO:: should not clean downsampled depth RT
			for (int j = 0; j < _countof(PipelineStates::deferredRTFmts); ++j) {
				commandList->ClearRenderTargetView(handle, Black, 0, nullptr);
				handle.Offset(rtv_.desc.GetDescriptorSize());
			}
		}
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_deviceResources->GetDepthStencilView();
		commandList->OMSetRenderTargets(_countof(PipelineStates::deferredRTFmts), &rtv_.entry.cpuHandle, true, &depthStencilView);
		commandList->SetPipelineState(state.pipelineState.Get());
		// Set the graphics root signature and descriptor heaps to be used by this frame.
		commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	}
}

bool Renderer::Render() {
	if (!loadingComplete_) return false;

	// TODO:: fixed size array
	std::vector<ID3D12CommandList*> ppCommandLists;
	for (auto& commandList : commandLists_) {
		commandList->Close();
		ppCommandLists.push_back(commandList.Get());
	}
	deferredCommandList_->Close();
	ppCommandLists.push_back(deferredCommandList_.Get());
	// Execute the command list.
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists((UINT)ppCommandLists.size(), &ppCommandLists.front());
	return true;
}
void Renderer::Submit(const ShaderStructures::BgCmd& cmd) {
	if (!loadingComplete_) return;
	auto id = cmd.shader;
	auto& state = pipelineStates_.states_[id];
	ID3D12GraphicsCommandList* commandList = commandLists_[id].Get();
	PIXBeginEvent(commandList, 0, L"SubmitBgCmd");
	const int descSize = frame_->desc.GetDescriptorSize();
	DescriptorFrameAlloc::Entry entry = frame_->desc.Push(3);
	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	auto cb = frame_->cb.Alloc(sizeof(float4x4));
	memcpy(cb.cpuAddress, &cmd.vp, sizeof(float4x4));
	frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
	UINT index = 0;
	commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
	++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
	frame_->desc.CreateCubeSRV(entry.cpuHandle, buffers_[cmd.cubeEnv].resource.Get());
	commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
	++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	assert(cmd.vb != InvalidBuffer);
	assert(cmd.ib != InvalidBuffer);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
		{ buffers_[cmd.vb].bufferLocation + cmd.submesh.vbByteOffset,
		(UINT)buffers_[cmd.vb].size - +cmd.submesh.vbByteOffset,
		cmd.submesh.stride } };

	commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);

	if (cmd.ib != InvalidBuffer) {
		D3D12_INDEX_BUFFER_VIEW	indexBufferView;
		{
			const auto& buffer = buffers_[cmd.ib];
			indexBufferView.BufferLocation = buffer.bufferLocation + cmd.submesh.ibByteOffset;
			indexBufferView.SizeInBytes = (UINT)buffer.size - cmd.submesh.ibByteOffset;
			indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		}

		commandList->IASetIndexBuffer(&indexBufferView);
		commandList->DrawIndexedInstanced(cmd.submesh.count, 1, 0, 0, 0);
	}
	else {
		commandList->DrawInstanced(cmd.submesh.count, 1, cmd.submesh.vbByteOffset, 0);
	}
	PIXEndEvent(commandList);
}
void Renderer::Submit(const DrawCmd& cmd) {
	if (!loadingComplete_) return;
	auto id = cmd.shader;
	auto& state = pipelineStates_.states_[id];
	ID3D12GraphicsCommandList* commandList = commandLists_[id].Get();
	PIXBeginEvent(commandList, 0, L"SubmitDrawCmd"); 
	{
		DescriptorFrameAlloc::Entry entry;
		UINT index = 0;
		const int descSize = frame_->desc.GetDescriptorSize();
		switch (id) {
		case ShaderStructures::Debug: {
			entry = frame_->desc.Push(2);
			ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			{
				auto cb = frame_->cb.Alloc(sizeof(float4x4));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				memcpy(cb.cpuAddress, &cmd.mvp, sizeof(float4x4));
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			{
				auto cb = frame_->cb.Alloc(sizeof(float4));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				memcpy(cb.cpuAddress, &cmd.material.diffuse, sizeof(float4));
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			break;
		}
		case ShaderStructures::Pos: {
			entry = frame_->desc.Push(2);
			ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			{
				auto cb = frame_->cb.Alloc(sizeof(Object));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((Object*)cb.cpuAddress)->m = cmd.m;
				((Object*)cb.cpuAddress)->mvp = cmd.mvp;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			{
				auto cb = frame_->cb.Alloc(sizeof(Material));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((Material*)cb.cpuAddress)->diffuse = cmd.material.diffuse;
				((Material*)cb.cpuAddress)->metallic_roughness = cmd.material.metallic_roughness;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			break;
		}
		case ShaderStructures::Tex: {
			entry = frame_->desc.Push(3);
			ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			{
				auto cb = frame_->cb.Alloc(sizeof(Object));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((Object*)cb.cpuAddress)->m = cmd.m;
				((Object*)cb.cpuAddress)->mvp = cmd.mvp;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			{
				auto& texture = buffers_[cmd.submesh.texAlbedo];
				frame_->desc.CreateSRV(entry.cpuHandle, texture.resource.Get());
			}
			entry.cpuHandle.Offset(descSize);
			{
				auto cb = frame_->cb.Alloc(sizeof(Material));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((Material*)cb.cpuAddress)->diffuse = cmd.material.diffuse;
				((Material*)cb.cpuAddress)->metallic_roughness = cmd.material.metallic_roughness;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			break;
		}
		default: assert(false);
		}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		assert(cmd.vb != InvalidBuffer);
		assert(cmd.ib != InvalidBuffer);
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
			{ buffers_[cmd.vb].bufferLocation + cmd.submesh.vbByteOffset,
			(UINT)buffers_[cmd.vb].size - +cmd.submesh.vbByteOffset,
			cmd.submesh.stride } };

		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);

		if (cmd.ib != InvalidBuffer) {
			D3D12_INDEX_BUFFER_VIEW	indexBufferView;
			{
				const auto& buffer = buffers_[cmd.ib];
				indexBufferView.BufferLocation = buffer.bufferLocation + cmd.submesh.ibByteOffset;
				indexBufferView.SizeInBytes = (UINT)buffer.size - cmd.submesh.ibByteOffset;
				indexBufferView.Format = DXGI_FORMAT_R16_UINT;
			}

			commandList->IASetIndexBuffer(&indexBufferView);
			commandList->DrawIndexedInstanced(cmd.submesh.count, 1, 0, 0, 0);
		}
		else {
			commandList->DrawInstanced(cmd.submesh.count, 1, cmd.submesh.vbByteOffset, 0);
		}
	}
	PIXEndEvent(commandList);
}
void Renderer::Submit(const ModoDrawCmd& cmd) {
	if (!loadingComplete_) return;
	auto id = cmd.shader;
	auto& state = pipelineStates_.states_[id];
	ID3D12GraphicsCommandList* commandList = commandLists_[id].Get();
	PIXBeginEvent(commandList, 0, L"SubmitDrawCmd"); 
	{
		DescriptorFrameAlloc::Entry entry;
		UINT index = 0;
		const int descSize = frame_->desc.GetDescriptorSize();
		switch (id) {
		case ShaderStructures::Pos: {
			entry = frame_->desc.Push(2);
			ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			{
				auto cb = frame_->cb.Alloc(sizeof(Object));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((Object*)cb.cpuAddress)->m = cmd.o.m;
				((Object*)cb.cpuAddress)->mvp = cmd.o.mvp;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			{
				auto cb = frame_->cb.Alloc(sizeof(Material));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((Material*)cb.cpuAddress)->diffuse = cmd.material.diffuse;
				((Material*)cb.cpuAddress)->metallic_roughness = cmd.material.metallic_roughness;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			break;
		}
		case ShaderStructures::ModoDN: {
			entry = frame_->desc.Push(2 + __popcnt(cmd.submesh.textureMask));
			ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			{
				auto cb = frame_->cb.Alloc(sizeof(Object));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((Object*)cb.cpuAddress)->m = cmd.o.m;
				((Object*)cb.cpuAddress)->mvp = cmd.o.mvp;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			for (int i = 0; i < _countof(cmd.submesh.textures); ++i) {
				auto& texture = buffers_[cmd.submesh.textures[i].id];
				frame_->desc.CreateSRV(entry.cpuHandle, texture.resource.Get());
				entry.cpuHandle.Offset(descSize);
			}
			{
				auto cb = frame_->cb.Alloc(sizeof(Material));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((Material*)cb.cpuAddress)->diffuse = cmd.material.diffuse;
				((Material*)cb.cpuAddress)->metallic_roughness = cmd.material.metallic_roughness;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize); entry.gpuHandle.Offset(descSize);
			break;
		}
		default: assert(false);
		}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		assert(cmd.vb != InvalidBuffer);
		assert(cmd.ib != InvalidBuffer);
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
			{ buffers_[cmd.vb].bufferLocation + cmd.submesh.vertexByteOffset,
			(UINT)buffers_[cmd.vb].size - cmd.submesh.vertexByteOffset,
			cmd.submesh.stride } };

		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);

		if (cmd.ib != InvalidBuffer) {
			D3D12_INDEX_BUFFER_VIEW	indexBufferView;
			{
				const auto& buffer = buffers_[cmd.ib];
				indexBufferView.BufferLocation = buffer.bufferLocation + cmd.submesh.indexByteOffset;
				indexBufferView.SizeInBytes = (UINT)buffer.size - cmd.submesh.indexByteOffset;
				indexBufferView.Format = DXGI_FORMAT_R16_UINT;
			}

			commandList->IASetIndexBuffer(&indexBufferView);
			commandList->DrawIndexedInstanced(cmd.submesh.count, 1, 0, 0, 0);
		} else {
			commandList->DrawInstanced(cmd.submesh.count, 1, cmd.submesh.vertexByteOffset, 0);
		}
	}
	PIXEndEvent(commandList);
}
void Renderer::DownsampleDepth(ID3D12GraphicsCommandList* commandList) {
	PIXBeginEvent(commandList, 0, L"DownsampleDepth");
	CD3DX12_RESOURCE_BARRIER barriers[] = { CD3DX12_RESOURCE_BARRIER::Transition(halfResDepth_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetDepthStencil(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
	commandList->ResourceBarrier(_countof(barriers), barriers);
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	const float factor = .5f;
	const float step = 2.f;
	viewport.Width *= factor; viewport.Height *= factor;
	commandList->RSSetViewports(1, &viewport);
	D3D12_RECT scissorRect = scissorRect_;
	scissorRect.right *= factor; scissorRect.bottom *= factor;
	commandList->RSSetScissorRects(1, &scissorRect);
	auto& state = pipelineStates_.states_[Downsample];
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	commandList->SetPipelineState(state.pipelineState.Get());
	commandList->OMSetRenderTargets(1, &rtv_.halfResDepthEntry.cpuHandle, false, nullptr);

	// TODO:: why can't set this??? ==> DXGI_ANALYSIS screws up
	auto cb = frame_->cb.Alloc(sizeof(float));
	*((float*)cb.cpuAddress) = step;
	memcpy(cb.cpuAddress, &step, sizeof(step));

	auto entry = frame_->desc.Push(2);
	frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
	const int descSize = frame_->desc.GetDescriptorSize();
	entry.cpuHandle.Offset(descSize);
	frame_->desc.CreateSRV(entry.cpuHandle, m_deviceResources->GetDepthStencil());
	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	commandList->SetGraphicsRootDescriptorTable(0, entry.gpuHandle);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	assert(fsQuad_ != InvalidBuffer);
	
	D3D12_VERTEX_BUFFER_VIEW vbv[] = {
		{ buffers_[fsQuad_].bufferLocation,
		(UINT)buffers_[fsQuad_].size,
		(UINT)sizeof(VertexFSQuad) } };

	commandList->IASetVertexBuffers(0, _countof(vbv), vbv);
	commandList->DrawInstanced(4, 1, 0, 0);
	PIXEndEvent(commandList);
}
void Renderer::DoLightingPass(const ShaderStructures::DeferredCmd& cmd) {
	if (!loadingComplete_) return;
	ID3D12GraphicsCommandList* commandList = deferredCommandList_.Get();

	PIXBeginEvent(commandList, 0, L"DoLightingPass");
	{
		CD3DX12_RESOURCE_BARRIER presentResourceBarriers[RenderTargetCount + 1];
		for (int i = 0; i < RenderTargetCount; ++i)
			presentResourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(rtt_[i].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		DownsampleDepth(commandList);
		presentResourceBarriers[RenderTargetCount] = CD3DX12_RESOURCE_BARRIER::Transition(halfResDepth_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		// TODO:: done in startForwardPass presentResourceBarriers[RenderTargetCount + 1] = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect_);
		commandList->ResourceBarrier(_countof(presentResourceBarriers), presentResourceBarriers);
		auto& state = pipelineStates_.states_[DeferredPBR];
		commandList->SetGraphicsRootSignature(state.rootSignature.Get());
		commandList->SetPipelineState(state.pipelineState.Get());
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_deviceResources->GetRenderTargetView();
		commandList->OMSetRenderTargets(1, &rtv, false, nullptr);

		const int descSize = frame_->desc.GetDescriptorSize();
		auto entry = frame_->desc.Push(2 + BINDING_COUNT);
		{
			// Scene
			auto cb = frame_->cb.Alloc(sizeof(cmd.scene));
			memcpy(cb.cpuAddress, &cmd.scene, sizeof(cmd.scene));
			frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
			entry.cpuHandle.Offset(descSize);
		}
		{
			// AO
			auto cb = frame_->cb.Alloc(sizeof(cmd.ao));
			memcpy(cb.cpuAddress, &cmd.ao, sizeof(cmd.ao));
			frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
			entry.cpuHandle.Offset(descSize);
		}
		// RTs
		for (int j = 0; j < _countof(PipelineStates::deferredRTFmts); ++j) {
			frame_->desc.CreateSRV(entry.cpuHandle, rtt_[j].Get());
			entry.cpuHandle.Offset(descSize);
		}
		// Depth
		frame_->desc.CreateSRV(entry.cpuHandle, m_deviceResources->GetDepthStencil());
		entry.cpuHandle.Offset(descSize);

		// Irradiance
		frame_->desc.CreateCubeSRV(entry.cpuHandle, buffers_[cmd.irradiance].resource.Get());
		entry.cpuHandle.Offset(descSize);
		// Prefiltered env. map
		frame_->desc.CreateCubeSRV(entry.cpuHandle, buffers_[cmd.prefilteredEnvMap].resource.Get());
		entry.cpuHandle.Offset(descSize);
		// BRDFLUT
		frame_->desc.CreateSRV(entry.cpuHandle, buffers_[cmd.BRDFLUT].resource.Get());
		entry.cpuHandle.Offset(descSize);
		// Halfresdepth
		frame_->desc.CreateSRV(entry.cpuHandle, halfResDepth_.Get());
		entry.cpuHandle.Offset(descSize);
		// Random
		frame_->desc.CreateSRV(entry.cpuHandle, buffers_[cmd.random].resource.Get());
		entry.cpuHandle.Offset(descSize);

		ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		commandList->SetGraphicsRootDescriptorTable(0, entry.gpuHandle);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		assert(fsQuad_ != InvalidBuffer);
	
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
			{ buffers_[fsQuad_].bufferLocation,
			(UINT)buffers_[fsQuad_].size,
			(UINT)sizeof(VertexFSQuad) } };

		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
		commandList->DrawInstanced(4, 1, 0, 0);

		// Indicate that the render target will now be used to present when the command list is done executing.
		CD3DX12_RESOURCE_BARRIER presentResourceBarrier[] =
		{ CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT),
		 CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetDepthStencil(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE) };
		
		commandList->ResourceBarrier(_countof(presentResourceBarrier), presentResourceBarrier);
	}
	PIXEndEvent(commandList);
}
