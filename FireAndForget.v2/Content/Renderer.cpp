#include "pch.h"
#include "Renderer.h"
#include "Content/UI.h"
#include "StringUtil.h"
#include "..\Common\DirectXHelper.h"
#include "SIMDTypeAliases.h"
#include "..\source\cpp\VertexTypes.h"
#include "..\source\cpp\DeferredBindings.h"
#include "..\..\source\cpp\BufferUtils.h"
#include "..\..\source\cpp\ShaderStructures.h"
#include <glm/gtc/matrix_transform.hpp>
// TODO::
// - all embedded rootsignatures are read from the ps source
// - depthstencil transition to depth write happens at the present resource barrier at the very end
// - DownsampleDepth transitions depth stencil to pixel shader resouce
// - remove deviceResource commandallocators
// - remove rtvs[framecount]
// - swap offset and frame in createsrv
// - fix rootparamindex counting via templates (1 root paramindex for vs, 1 for ps instead of buffercount)
// - rename: shaders “scene.vs.hlsl” and “scene.ps.hlsl”, I create a third file “scene.rs.hlsli”, which contains two things:
// - simplify RootSignature access for SetGraphicsRootSignature

#define DEBUG_PIX
#ifdef DEBUG_PIX
#define PIX(x) x
#else
#define PIX(x)
#endif
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
}
Renderer::Renderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, DXGI_FORMAT backbufferFormat) :
	backbufferFormat_(backbufferFormat),
	pipelineStates_(deviceResources->GetD3DDevice(), backbufferFormat, depthbufferFormat_),
	m_deviceResources(deviceResources) {

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

Renderer::~Renderer() {
	UI::Shutdown();
}

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
	UI::Init(device, m_deviceResources->GetSwapChain());

	rtvDescAlloc_.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, defaultRTVDescCount, false);
	dsvDescAlloc_.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

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

	//BeginUploadResources();
	//{
	//	// fullscreen quad...
	//	VertexFSQuad quad[] = { { { -1., -1.f },{ 0.f, 1.f } },{ { -1., 1.f },{ 0.f, 0.f } },{ { 1., -1.f },{ 1.f, 1.f } },{ { 1., 1.f },{ 1.f, 0.f } } };
	//	fsQuad_ = CreateBuffer(quad, sizeof(quad));
	//}
	//EndUploadResources();
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
namespace {
	struct CubeViewResult {
		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
	};
	CubeViewResult GenCubeViews(DescriptorFrameAlloc::Entry& entry, DescriptorFrameAlloc& descAlloc, CBFrameAlloc& cbAlloc) {
		CubeViewResult result;
		constexpr int faceCount = 6;
		const auto descSize = descAlloc.GetDescriptorSize();
		// cube view matrices for cube env. map generation
		const glm::vec3 at[] = {/*+x*/{1.f, 0.f, 0.f},/*-x*/{-1.f, 0.f, 0.f},/*+y*/{0.f, 1.f, 0.f},/*-y*/{0.f, -1.f, 0.f},/*+z*/{0.f, 0.f, 1.f},/*-z*/{0.f, 0.f, -1.f} },
			up[] = { {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f} };
		const int inc = AlignTo<256, int>(sizeof(glm::mat4x4)), vsSize = inc * faceCount;
		auto cb = cbAlloc.Alloc(vsSize);
		uint8_t* p = cb.cpuAddress;
		result.cpuHandle = entry.cpuHandle;
		result.gpuAddress = cb.gpuAddress;
		for (int i = 0; i < faceCount; ++i, p += inc) {
			glm::mat4x4 v = glm::lookAtLH({ 0.f, 0.f, 0.f }, at[i], up[i]);
			memcpy(p, &v, sizeof(v));
			descAlloc.CreateCBV(result.cpuHandle, result.gpuAddress, inc);
			result.cpuHandle.Offset(descSize); result.gpuAddress += inc;
		}
		return result;
	}
}
TextureIndex Renderer::GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const ModoMeshLoader::Submesh& submesh, uint32_t dim, ShaderId shader, bool mip, LPCSTR label) {
	assert(vb != InvalidBuffer);
	assert(ib != InvalidBuffer);
	auto device = m_deviceResources->GetD3DDevice();
	DXGI_FORMAT fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const int faceCount = 6;
	ComPtr<ID3D12Resource> resource;
	{
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(fmt, dim, dim, faceCount, (mip) ? UINT16(NumMips(dim, dim)) : 1);
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
		auto entry = prePass_.rtv.desc.Push(faceCount);
		rtvHandle = entry.cpuHandle;
		auto handle = rtvHandle;
		for (int i = 0; i < faceCount; ++i) {
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
	auto entry = prePass_.desc.Push(faceCount + 1);
	CD3DX12_GPU_DESCRIPTOR_HANDLE vsGPUHandle = entry.gpuHandle;
	CubeViewResult result = GenCubeViews(entry, prePass_.desc, prePass_.cb);

	CD3DX12_GPU_DESCRIPTOR_HANDLE psGPUHandle = entry.gpuHandle; psGPUHandle.Offset(faceCount * descSize); // for ps srv binding
	if (buffers_[tex].resource->GetDesc().DepthOrArraySize > 1)
		prePass_.desc.CreateCubeSRV(result.cpuHandle, buffers_[tex].resource.Get());
	else
		prePass_.desc.CreateSRV(result.cpuHandle, buffers_[tex].resource.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews = { buffers_[vb].bufferLocation + submesh.vertexByteOffset,
			(UINT)buffers_[vb].size, submesh.stride } ;
	commandList->IASetVertexBuffers(0, 1, &vertexBufferViews);
	D3D12_INDEX_BUFFER_VIEW	indexBufferView = {
			buffers_[ib].bufferLocation + submesh.indexByteOffset,
			(UINT)buffers_[ib].size,
			DXGI_FORMAT_R16_UINT };
	commandList->IASetIndexBuffer(&indexBufferView);
	commandList->SetGraphicsRootDescriptorTable(1, psGPUHandle);
	for (int i = 0; i < faceCount; ++i) {
		commandList->SetGraphicsRootDescriptorTable(0, vsGPUHandle);
		vsGPUHandle.Offset(descSize);
		commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
		rtvHandle.Offset(rtvDescSize);
		commandList->DrawIndexedInstanced(submesh.count, 1, 0, 0, 0);
	}
	if (mip) {
		GenMips(resource, fmt, dim, dim, faceCount);
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
			prePass_.desc.CreateCBV(handle, cb.gpuAddress, AlignTo<256, uint32_t>(sizeof(cbuffer)));
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
			commandList->Dispatch(AlignTo<kNumThreads>(w) / kNumThreads, AlignTo<kNumThreads>(h) / kNumThreads, 1);
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

TextureIndex Renderer::GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const ModoMeshLoader::Submesh& submesh, uint32_t dim, ShaderId shader, LPCSTR label) {
	assert(vb != InvalidBuffer);
	assert(ib != InvalidBuffer);
	auto device = m_deviceResources->GetD3DDevice();
	DXGI_FORMAT fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const int psBindingCount = 3;
	const int faceCount = 6, mipLevelCount = 5, count = faceCount + (mipLevelCount * psBindingCount);
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
	CubeViewResult result = GenCubeViews(entry, prePass_.desc, prePass_.cb);
	
	CD3DX12_GPU_DESCRIPTOR_HANDLE psGPUHandle = entry.gpuHandle; psGPUHandle.Offset(faceCount * descSize); // for ps bindings
	auto handle = entry.cpuHandle; handle.Offset(faceCount * descSize);
	// roughness
	auto cb0 = prePass_.cb.Alloc(AlignTo<256, int>(sizeof(float)) * mipLevelCount);
	const int inc0 = AlignTo<256, int>(sizeof(float));
	float* p0 = (float*)cb0.cpuAddress;
	auto gpuAddress = cb0.gpuAddress;

	// resolution
	auto cb1 = prePass_.cb.Alloc(sizeof(float));
	const int inc1 = AlignTo<256, int>(sizeof(float));
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
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews = { buffers_[vb].bufferLocation + submesh.vertexByteOffset,
			(UINT)buffers_[vb].size, submesh.stride } ;
	commandList->IASetVertexBuffers(0, 1, &vertexBufferViews);
	D3D12_INDEX_BUFFER_VIEW	indexBufferView = {
			buffers_[ib].bufferLocation + submesh.indexByteOffset,
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

	//assert(fsQuad_ != InvalidBuffer);
	//
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
	//	{ buffers_[fsQuad_].bufferLocation,
	//	(UINT)buffers_[fsQuad_].size,
	//	(UINT)sizeof(VertexFSQuad) } };

	//commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
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
	auto size = m_deviceResources->GetOutputSize();
	UI::OnResize(device, m_deviceResources->GetSwapChain(), size.Width, size.Height);

	// Create render target views of the swap chain back buffer.
	{
		renderTargets_.view = rtvDescAlloc_.Push(ShaderStructures::FrameCount);
		renderTargets_.width = lround(size.Width); renderTargets_.height = lround(size.Height);
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = renderTargets_.view.cpuHandle;
		for (UINT n = 0; n < ShaderStructures::FrameCount; n++) {
			DX::ThrowIfFailed(m_deviceResources->GetSwapChain()->GetBuffer(n, IID_PPV_ARGS(&renderTargets_.resource[n])));
			device->CreateRenderTargetView(renderTargets_.resource[n].Get(), nullptr, handle);
			handle.Offset(rtvDescAlloc_.GetDescriptorSize());
			renderTargets_.state[n] = D3D12_RESOURCE_STATE_PRESENT;
			WCHAR name[25];
			if (swprintf_s(name, L"renderTargets[%u]", n) > 0) DX::SetName(renderTargets_.resource[n].Get(), name);
		}
	}

	// Create a depth stencil and view.
	{
		depthStencil_.view = dsvDescAlloc_.Push(1);
		D3D12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		depthStencil_.width = lround(size.Width); depthStencil_.height = lround(size.Height);
		D3D12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthresourceFormat_, depthStencil_.width, depthStencil_.height, 1, 1);
		depthResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		depthStencil_.state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		CD3DX12_CLEAR_VALUE depthOptimizedClearValue(depthbufferFormat_, 1.0f, 0);
		DX::ThrowIfFailed(device->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthResourceDesc,
			depthStencil_.state,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&depthStencil_.resource)
			));

		NAME_D3D12_OBJECT(depthStencil_.resource);

		dsvDescAlloc_.CreateDSV(depthStencil_.view.cpuHandle, depthStencil_.resource.Get(), depthbufferFormat_);
	}

	D3D12_CLEAR_VALUE clearValue;
	memcpy(clearValue.Color, Black, sizeof(clearValue.Color));
	rtt_.view = rtvDescAlloc_.Push(_countof(PipelineStates::deferredRTFmts));
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle = rtt_.view.cpuHandle;
	rtt_.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	rtt_.width = lround(size.Width); rtt_.height = lround(size.Height);
	for (int j = 0; j < _countof(PipelineStates::deferredRTFmts); ++j) {
		clearValue.Format = PipelineStates::deferredRTFmts[j];
		WCHAR label[25];
		swprintf_s(label, L"renderTarget[%u]", j);
		rtt_.res[j] = CreateRenderTarget(PipelineStates::deferredRTFmts[j], rtt_.width, rtt_.height, rtt_.state, &clearValue, label);
		rtvDescAlloc_.CreateRTV(handle, rtt_.res[j].Get(), PipelineStates::deferredRTFmts[j]);
		handle.Offset(rtvDescAlloc_.GetDescriptorSize());
	}

	auto halfWidth = lround(size.Width) >> 1, halfHeight = lround(size.Height) >> 1;
	// half-res depth
	{
		halfResDepth_.width = halfWidth; halfResDepth_.height = halfHeight;
		DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
		halfResDepth_.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		halfResDepth_.resource = CreateRenderTarget(format, halfResDepth_.width, halfResDepth_.height, halfResDepth_.state, nullptr, L"halfResDepth");
		halfResDepth_.view = rtvDescAlloc_.Push(1);
		rtvDescAlloc_.CreateRTV(halfResDepth_.view.cpuHandle, halfResDepth_.resource.Get(), format);
	}

	// ssao
	{
		ssao_.width = halfWidth; ssao_.height = halfHeight;
		DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
		ssao_.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		ssao_.resource = CreateRenderTarget(format, ssao_.width, ssao_.height, ssao_.state, nullptr, L"aoRT");
		ssao_.view = rtvDescAlloc_.Push(1);
		rtvDescAlloc_.CreateRTV(ssao_.view.cpuHandle, ssao_.resource.Get(), format);
	}
}
Microsoft::WRL::ComPtr<ID3D12Resource> Renderer::CreateRenderTarget(DXGI_FORMAT format, UINT width, UINT height, D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clearValue, LPCWSTR label) {
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
	desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		ssao_.state,
		clearValue,
		IID_PPV_ARGS(&resource)));
	if (label) DX::SetName(resource.Get(), label);
	return resource;
}
void Renderer::Update(double frame, double total) {
	if (loadingComplete_){
		UI::Update(frame, total);
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
	commandList->RSSetViewports(1, &renderTargets_.GetViewport());
	commandList->RSSetScissorRects(1, &renderTargets_.GetScissorRect());
	commandList->ClearDepthStencilView(depthStencil_.view.cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void Renderer::StartForwardPass() {
	if (!loadingComplete_) return;
	bool first = true;
	for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
		auto& state = pipelineStates_.states_[i];
		if (state.pass != PipelineStates::State::RenderPass::Forward) continue;
		auto* commandList = commandLists_[i].Get();
		commandList->RSSetViewports(1, &renderTargets_.GetViewport());
		commandList->RSSetScissorRects(1, &renderTargets_.GetScissorRect());
		if (first) {
			first = false;

			auto afterState = D3D12_RESOURCE_STATE_RENDER_TARGET;
			auto& beforeState = renderTargets_.state[GetCurrenFrameIndex()];
			if (beforeState != afterState) {
				auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(GetRenderTarget(), beforeState, afterState);
				beforeState = afterState;
				commandList->ResourceBarrier(1, &barrier);
			}

			commandList->ClearRenderTargetView(GetRenderTargetView(), Black, 0, nullptr);
		}
		commandList->OMSetRenderTargets(1, &GetRenderTargetView(), false, &depthStencil_.view.cpuHandle);
		commandList->SetPipelineState(state.pipelineState.Get());
		commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	}
}

void Renderer::StartGeometryPass() {
	if (!loadingComplete_) return;
	// assign pipelinestates with command lists
	bool first = true;
	for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
		auto& state = pipelineStates_.states_[i];
		if (state.pass != PipelineStates::State::RenderPass::Geometry) continue;
		auto* commandList = commandLists_[i].Get();
		commandList->RSSetViewports(1, &rtt_.GetViewport());
		commandList->RSSetScissorRects(1, &rtt_.GetScissorRect());
		if (first) {
			first = false;
			CD3DX12_RESOURCE_BARRIER barriers[_countof(PipelineStates::deferredRTFmts)];
			for (int i = 0; i < _countof(barriers); ++i) {
				barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(rtt_.res[i].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
			}
			commandList->ResourceBarrier(_countof(barriers), barriers);
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(rtt_.view.cpuHandle);
			// TODO:: should not clean downsampled depth RT
			for (int j = 0; j < _countof(PipelineStates::deferredRTFmts); ++j) {
				commandList->ClearRenderTargetView(handle, Black, 0, nullptr);
				handle.Offset(rtvDescAlloc_.GetDescriptorSize());
			}
		}
		commandList->OMSetRenderTargets(_countof(PipelineStates::deferredRTFmts), &rtt_.view.cpuHandle, true, &depthStencil_.view.cpuHandle);
		commandList->SetPipelineState(state.pipelineState.Get());
		// Set the graphics root signature and descriptor heaps to be used by this frame.
		commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	}
}

bool Renderer::Render() {
	if (!loadingComplete_) return false;

	// TODO:: fixed size array
	std::vector<ID3D12CommandList*> ppCommandLists;
	for (auto& commandList : commandLists_)
		ppCommandLists.push_back(commandList.Get());
	ppCommandLists.push_back(deferredCommandList_.Get());

	ppCommandLists.push_back(UI::Render(m_deviceResources->GetSwapChain()->GetCurrentBackBufferIndex()));

	auto commandList = (ID3D12GraphicsCommandList*)ppCommandLists.back();
	renderTargets_.ResourceTransition(commandList, GetCurrenFrameIndex(), D3D12_RESOURCE_STATE_PRESENT);
	depthStencil_.ResourceTransition(commandList,  D3D12_RESOURCE_STATE_DEPTH_WRITE);

	for (auto* commandList : ppCommandLists)
		((ID3D12GraphicsCommandList*)commandList)->Close();

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
		{ buffers_[cmd.vb].bufferLocation + cmd.submesh.vertexByteOffset,
		(UINT)buffers_[cmd.vb].size - +cmd.submesh.vertexByteOffset,
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
	}
	else {
		commandList->DrawInstanced(cmd.submesh.count, 1, cmd.submesh.vertexByteOffset, 0);
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
	PIXBeginEvent(commandList, 0, L"SubmitModoDrawCmd"); 
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
				if (cmd.submesh.textureMask & (1 << i)) {
					auto& texture = buffers_[cmd.submesh.textures[i].id];
					frame_->desc.CreateSRV(entry.cpuHandle, texture.resource.Get());
					entry.cpuHandle.Offset(descSize);
				}
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
		case ShaderStructures::ModoDNMR:{
			entry = frame_->desc.Push(1 + __popcnt(cmd.submesh.textureMask));
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
				if (cmd.submesh.textureMask & (1 << i)) {
					auto& texture = buffers_[cmd.submesh.textures[i].id];
					frame_->desc.CreateSRV(entry.cpuHandle, texture.resource.Get());
					entry.cpuHandle.Offset(descSize);
				}
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
	PIX(PIXBeginEvent(commandList, 0, L"DownsampleDepth"));

	halfResDepth_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	depthStencil_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->RSSetViewports(1, &halfResDepth_.GetViewport());
	commandList->RSSetScissorRects(1, &halfResDepth_.GetScissorRect());
	auto& state = pipelineStates_.states_[Downsample];
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	commandList->SetPipelineState(state.pipelineState.Get());
	commandList->OMSetRenderTargets(1, &halfResDepth_.view.cpuHandle, false, nullptr);

	// TODO:: why can't set this??? ==> DXGI_ANALYSIS screws up
	auto cb = frame_->cb.Alloc(sizeof(float));
	*((float*)cb.cpuAddress) = (float)depthStencil_.width / halfResDepth_.width;

	auto entry = frame_->desc.Push(2);
	frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
	const int descSize = frame_->desc.GetDescriptorSize();
	entry.cpuHandle.Offset(descSize);
	frame_->desc.CreateSRV(entry.cpuHandle, depthStencil_.resource.Get());
	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	commandList->SetGraphicsRootDescriptorTable(0, entry.gpuHandle);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//assert(fsQuad_ != InvalidBuffer);
	//
	//D3D12_VERTEX_BUFFER_VIEW vbv[] = {
	//	{ buffers_[fsQuad_].bufferLocation,
	//	(UINT)buffers_[fsQuad_].size,
	//	(UINT)sizeof(VertexFSQuad) } };

	//commandList->IASetVertexBuffers(0, _countof(vbv), vbv);
	commandList->DrawInstanced(4, 1, 0, 0);
	halfResDepth_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PIX(PIXEndEvent(commandList));
}

void Renderer::SSAOPass(ID3D12GraphicsCommandList* commandList) {
	PIX(PIXBeginEvent(commandList, 0, L"SSAOPASS"));
	auto* depth = &halfResDepth_;
	depth->ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	ssao_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	auto& state = pipelineStates_.states_[SSAO];
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	commandList->SetPipelineState(state.pipelineState.Get());
	commandList->RSSetViewports(1, &ssao_.GetViewport());
	commandList->RSSetScissorRects(1, &ssao_.GetScissorRect());
	commandList->DrawInstanced(4, 1, 0, 0);
	ssao_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PIX(PIXEndEvent(commandList));
}
void Renderer::DoLightingPass(const ShaderStructures::DeferredCmd& cmd) {
	if (!loadingComplete_) return;
	ID3D12GraphicsCommandList* commandList = deferredCommandList_.Get();

	PIX(PIXBeginEvent(commandList, 0, L"DoLightingPass"));
	CD3DX12_RESOURCE_BARRIER presentResourceBarriers[RenderTargetCount];
	for (int i = 0; i < RenderTargetCount; ++i)
		presentResourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(rtt_.res[i].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	DownsampleDepth(commandList);
	halfResDepth_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	// TODO:: done in startForwardPass presentResourceBarriers[RenderTargetCount + 1] = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	commandList->RSSetViewports(1, &renderTargets_.GetViewport());
	commandList->RSSetScissorRects(1, &renderTargets_.GetScissorRect());
	commandList->ResourceBarrier(_countof(presentResourceBarriers), presentResourceBarriers);
	auto& state = pipelineStates_.states_[DeferredPBR];
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	commandList->SetPipelineState(state.pipelineState.Get());
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRenderTargetView();
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
		frame_->desc.CreateSRV(entry.cpuHandle, rtt_.res[j].Get());
		entry.cpuHandle.Offset(descSize);
	}
	// Depth
	frame_->desc.CreateSRV(entry.cpuHandle, depthStencil_.resource.Get());
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
	frame_->desc.CreateSRV(entry.cpuHandle, halfResDepth_.resource.Get());
	entry.cpuHandle.Offset(descSize);
	// Random
	frame_->desc.CreateSRV(entry.cpuHandle, buffers_[cmd.random].resource.Get());
	entry.cpuHandle.Offset(descSize);

	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	commandList->SetGraphicsRootDescriptorTable(0, entry.gpuHandle);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//assert(fsQuad_ != InvalidBuffer);
	
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
	//	{ buffers_[fsQuad_].bufferLocation,
	//	(UINT)buffers_[fsQuad_].size,
	//	(UINT)sizeof(VertexFSQuad) } };

	//commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
	commandList->DrawInstanced(4, 1, 0, 0);

	PIXEndEvent(commandList);
}
