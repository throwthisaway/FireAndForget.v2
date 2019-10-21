#include "pch.h"
#include "Renderer.h"
#include "Content/UI.h"
#include "StringUtil.h"
#include "D3DHelpers.h"
#include "..\Common\DirectXHelper.h"
#include "SIMDTypeAliases.h"
#include "..\source\cpp\VertexTypes.h"
#include "..\..\source\cpp\BufferUtils.h"
#include "..\..\source\cpp\ShaderStructures.h"
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <array>

// TODO::
// - polygon import crashes on beethoveen subdivided twice
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
	std::random_device rd;
	std::mt19937 mt(rd());
	static constexpr size_t defaultCBFrameAllocSize = 16384, defaultDescFrameAllocCount = 64,
		defaultRTVDescCount = 32;
	inline uint32_t NumMips(uint32_t w, uint32_t h) {
		uint32_t res;
		_BitScanReverse((unsigned long*)& res, w | h);
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
	DEBUGUI(ui::Shutdown());
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
	DEBUGUI(ui::Init(device, m_deviceResources->GetSwapChain()));

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

	pipelineStates_.completionTask.then([this]() {
		loadingComplete_ = true;
	});

	//prepass
	DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&prePass_.cmdAllocator)));
	DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, prePass_.cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&prePass_.cmdList)));
	prePass_.cmdList->Close();
	//DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&prePass_.computeCmdAllocator)));
	//DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, prePass_.computeCmdAllocator.Get(), nullptr, IID_PPV_ARGS(&prePass_.computeCmdList)));
	//prePass_.computeCmdList->Close();

	for (UINT n = 0; n < FrameCount; ++n) {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
		commandAllocators_[n] = commandAllocator;
	}
	DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators_[0].Get(), nullptr, IID_PPV_ARGS(&commandList_)));
	commandList_->Close();

	//BeginUploadResources();
	//{
	//	// fullscreen quad...
	//	VertexFSQuad quad[] = { { { -1., -1.f },{ 0.f, 1.f } },{ { -1., 1.f },{ 0.f, 0.f } },{ { 1., -1.f },{ 1.f, 1.f } },{ { 1., 1.f },{ 1.f, 0.f } } };
	//	fsQuad_ = CreateBuffer(quad, sizeof(quad));
	//}
	//EndUploadResources();
	auto kernel = SSAO::GenKernel();
	auto size = kernel.size() * sizeof(kernel.front());
	ssao_.kernelResource = CreateConstantBuffer(device, ssao_.size256 = AlignTo<256>(size), L"SSAO kernel");
	void* data;
	ssao_.kernelResource->Map(0, &CD3DX12_RANGE(0, 0), &data);
	memcpy(data, kernel.data(), size);
	ssao_.kernelResource->Unmap(0, &CD3DX12_RANGE(0, 0));

}
Windows::Foundation::Size Renderer::GetWindowSize() {
	return m_deviceResources->GetOutputSize();
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
	auto& state = pipelineStates_.states[shader];
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
			auto& state = pipelineStates_.states[shaderId];
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
	auto& state = pipelineStates_.states[shader];
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
	auto& state = pipelineStates_.states[shader];
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
void Renderer::BeforeResize() {
	m_deviceResources->WaitForGpu();
	DEBUGUI(ui::BeforeResize());
	for (int i = 0; i < _countof(backBufferRTs_.resources); ++i) backBufferRTs_.resources[i].Reset();
}
void Renderer::CreateWindowSizeDependentResources() {
	auto device = m_deviceResources->GetD3DDevice();
	auto size = m_deviceResources->GetOutputSize();
	DEBUGUI(ui::OnResize(device, m_deviceResources->GetSwapChain(), (int)size.Width, (int)size.Height));

	rtvDescAlloc_.Reset();
	dsvDescAlloc_.Reset();
	// Create render target views of the swap chain back buffer.
	{
		backBufferRTs_.view = rtvDescAlloc_.Push(ShaderStructures::FrameCount);
		backBufferRTs_.width = lround(size.Width); backBufferRTs_.height = lround(size.Height);
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = backBufferRTs_.view.cpuHandle;
		for (UINT n = 0; n < ShaderStructures::FrameCount; n++) {
			DX::ThrowIfFailed(m_deviceResources->GetSwapChain()->GetBuffer(n, IID_PPV_ARGS(&backBufferRTs_.resources[n])));
			device->CreateRenderTargetView(backBufferRTs_.resources[n].Get(), nullptr, handle);
			handle.Offset(rtvDescAlloc_.GetDescriptorSize());
			backBufferRTs_.states[n] = D3D12_RESOURCE_STATE_PRESENT;
			WCHAR name[25];
			if (swprintf_s(name, L"renderTargets[%u]", n) > 0) DX::SetName(backBufferRTs_.resources[n].Get(), name);
		}
	}

	// Create a depth stencil and view.
	depthStencil_ = CreateDepthRT(lround(size.Width), lround(size.Height));

	D3D12_CLEAR_VALUE clearValue;
	memcpy(clearValue.Color, Black, sizeof(clearValue.Color));
	gbuffersRT_.view = rtvDescAlloc_.Push(_countof(PipelineStates::deferredRTFmts));
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle = gbuffersRT_.view.cpuHandle;
	gbuffersRT_.width = lround(size.Width); gbuffersRT_.height = lround(size.Height);
	for (int j = 0; j < _countof(PipelineStates::deferredRTFmts); ++j) {
		gbuffersRT_.states[j] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		clearValue.Format = PipelineStates::deferredRTFmts[j];
		WCHAR label[25];
		swprintf_s(label, L"deferredRT[%u]", j);
		gbuffersRT_.resources[j] = CreateRenderTarget(PipelineStates::deferredRTFmts[j], gbuffersRT_.width, gbuffersRT_.height, gbuffersRT_.states[j], &clearValue, label);
		rtvDescAlloc_.CreateRTV(handle, gbuffersRT_.resources[j].Get(), PipelineStates::deferredRTFmts[j]);
		handle.Offset(rtvDescAlloc_.GetDescriptorSize());
	}

	{
		auto width = lround(size.Width) >> 1, height = lround(size.Height) >> 1;
		//auto width = lround(size.Width), height = lround(size.Height);
		// half-res depth
		{
			halfResDepthRT_.width = width; halfResDepthRT_.height = height;
			DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
			halfResDepthRT_.states[0] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			halfResDepthRT_.resources[0] = CreateRenderTarget(format, halfResDepthRT_.width, halfResDepthRT_.height, *halfResDepthRT_.states, nullptr, L"halfResDepth");
			halfResDepthRT_.view = rtvDescAlloc_.Push(1);
			rtvDescAlloc_.CreateRTV(halfResDepthRT_.view.cpuHandle, halfResDepthRT_.resources->Get(), format);
		}

		// ssao
		{
			auto entry = rtvDescAlloc_.Push(2);
			auto& rt = ssaoRT_;
			rt.width = width; rt.height = height;
			DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
			rt.states[0] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			rt.resources[0] = CreateRenderTarget(format, rt.width, rt.height, rt.states[0], nullptr, L"aoRT");
			rt.view = entry;
			rtvDescAlloc_.CreateRTV(entry.cpuHandle, rt.resources[0].Get(), format);

			entry.cpuHandle.Offset(rtvDescAlloc_.GetDescriptorSize());
			format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			rt.states[1] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			rt.resources[1] = CreateRenderTarget(format, rt.width, rt.height, rt.states[1], nullptr, L"aoDebugRT");
			rtvDescAlloc_.CreateRTV(entry.cpuHandle, rt.resources[1].Get(), format);
		}

		// ssao blur
		{
			auto& rt = ssaoBlurRT_;
			rt.width = width; rt.height = height;
			DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
			rt.states[0] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			rt.resources[0] = CreateRenderTarget(format, rt.width, rt.height, *rt.states, nullptr, L"ssaoBlurRT");
			rt.view = rtvDescAlloc_.Push(1);
			rtvDescAlloc_.CreateRTV(rt.view.cpuHandle, rt.resources->Get(), format);
		}
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
		state,
		clearValue,
		IID_PPV_ARGS(&resource)));
	if (label) DX::SetName(resource.Get(), label);
	return resource;
}

Renderer::RT<1> Renderer::CreateDepthRT(UINT width, UINT height) {
	RT<1> rt;
	rt.view = dsvDescAlloc_.Push(1);
	D3D12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	rt.width = lround(width); rt.height = lround(height);
	D3D12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthresourceFormat_, rt.width, rt.height, 1, 1);
	depthResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	rt.states[0] = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	CD3DX12_CLEAR_VALUE depthOptimizedClearValue(depthbufferFormat_, 1.0f, 0);
	auto device = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(device->CreateCommittedResource(
		&depthHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&depthResourceDesc,
		rt.states[0],
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&rt.resources[0])
		));

	NAME_D3D12_OBJECT(rt.resources[0]);

	dsvDescAlloc_.CreateDSV(rt.view.cpuHandle, rt.resources->Get(), depthbufferFormat_);
	return rt;
}
RTIndex Renderer::CreateShadowRT(UINT width, UINT height) {
	auto rt = CreateDepthRT(width, height);
	renderTargets_.push_back(rt);
	return (RTIndex)renderTargets_.size() - 1;
}
void Renderer::Update(double frame, double total) {
	if (loadingComplete_){
		DEBUGUI(ui::Update(frame, total));
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

	auto commandAllocator = commandAllocators_[m_deviceResources->GetCurrentFrameIndex()];
	DX::ThrowIfFailed(commandAllocator->Reset());
	DX::ThrowIfFailed(commandList_->Reset(commandAllocator.Get(), nullptr));
	DX::ThrowIfFailed(frame_->deferredCommandAllocator->Reset());
	DX::ThrowIfFailed(deferredCommandList_->Reset(frame_->deferredCommandAllocator.Get(), nullptr));
}

void Renderer::StartForwardPass() {
	if (!loadingComplete_) return;
	auto* commandList = commandList_.Get();
	commandList->RSSetViewports(1, &backBufferRTs_.GetViewport());
	commandList->RSSetScissorRects(1, &backBufferRTs_.GetScissorRect());
	commandList_->ClearDepthStencilView(depthStencil_.view.cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	auto afterState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	auto& beforeState = backBufferRTs_.states[GetCurrenFrameIndex()];
	if (beforeState != afterState) {
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(GetBackBuffer(), beforeState, afterState);
		beforeState = afterState;
		commandList->ResourceBarrier(1, &barrier);
	}
	commandList->OMSetRenderTargets(1, &GetBackBufferView(), false, &depthStencil_.view.cpuHandle);
	commandList->ClearRenderTargetView(GetBackBufferView(), Black, 0, nullptr);

}

void Renderer::StartGeometryPass() {
	if (!loadingComplete_) return;

	commandList_->RSSetViewports(1, &gbuffersRT_.GetViewport());
	commandList_->RSSetScissorRects(1, &gbuffersRT_.GetScissorRect());
	// TODO:: no need if StartForwardPass does the same
	commandList_->ClearDepthStencilView(depthStencil_.view.cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	gbuffersRT_.ResourceTransition(commandList_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(gbuffersRT_.view.cpuHandle);
	for (int j = 0; j < _countof(PipelineStates::deferredRTFmts); ++j) {
		commandList_->ClearRenderTargetView(handle, Black, 0, nullptr);
		handle.Offset(rtvDescAlloc_.GetDescriptorSize());
	}
	commandList_->OMSetRenderTargets(_countof(PipelineStates::deferredRTFmts), &gbuffersRT_.view.cpuHandle, true, &depthStencil_.view.cpuHandle);
}

bool Renderer::Render() {
	if (!loadingComplete_) return false;

	// TODO:: fixed size array
	std::vector<ID3D12CommandList*> ppCommandLists;
	ppCommandLists.push_back(commandList_.Get());
	ppCommandLists.push_back(deferredCommandList_.Get());

	DEBUGUI(ppCommandLists.push_back(ui::Render(m_deviceResources->GetSwapChain()->GetCurrentBackBufferIndex())));

	auto commandList = (ID3D12GraphicsCommandList*)ppCommandLists.back();
	backBufferRTs_.ResourceTransition(commandList, GetCurrenFrameIndex(), D3D12_RESOURCE_STATE_PRESENT);
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
	auto& state = pipelineStates_.states[id];
	ID3D12GraphicsCommandList* commandList = commandList_.Get();
	PIXBeginEvent(commandList, 0, L"SubmitBgCmd");
	commandList->SetPipelineState(state.pipelineState.Get());
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
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
	auto& state = pipelineStates_.states[id];
	ID3D12GraphicsCommandList* commandList = commandList_.Get();
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
	auto& state = pipelineStates_.states[id];
	ID3D12GraphicsCommandList* commandList = commandList_.Get();
	PIXBeginEvent(commandList, 0, L"SubmitModoDrawCmd"); 
	commandList->SetPipelineState(state.pipelineState.Get());
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	{
		DescriptorFrameAlloc::Entry entry;
		UINT index = 0;
		const int descSize = frame_->desc.GetDescriptorSize();
		switch (id) {
		case ShaderStructures::Pos: {
			entry = frame_->desc.Push(2);
			ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

			frame_->BindCBV(entry.cpuHandle, cmd.o);
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.gpuHandle.Offset(descSize);

			frame_->BindCBV(entry.cpuHandle, cmd.material);
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			break;
		}
		case ShaderStructures::ModoDN: {
			entry = frame_->desc.Push(2 + __popcnt(cmd.submesh.textureMask));
			ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

			frame_->BindCBV(entry.cpuHandle, cmd.o);
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.gpuHandle.Offset(descSize);

			for (int i = 0; i < _countof(cmd.submesh.textures); ++i) {
				if (cmd.submesh.textureMask & (1 << i)) {
					auto& texture = buffers_[cmd.submesh.textures[i].id];
					frame_->BindSRV(entry.cpuHandle, texture.resource.Get());
				}
			}
			frame_->BindCBV(entry.cpuHandle, cmd.material);
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			break;
		}
		case ShaderStructures::ModoDNMR:{
			entry = frame_->desc.Push(1 + __popcnt(cmd.submesh.textureMask));
			ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
			commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

			frame_->BindCBV(entry.cpuHandle, cmd.o);
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.gpuHandle.Offset(descSize);

			for (int i = 0; i < _countof(cmd.submesh.textures); ++i) {
				if (cmd.submesh.textureMask & (1 << i)) {
					auto& texture = buffers_[cmd.submesh.textures[i].id];
					frame_->BindSRV(entry.cpuHandle, texture.resource.Get());
				}
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
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
void Renderer::StartShadowPass(RTIndex rtIndex) {
	auto* commandList = commandList_.Get();
	PIX(PIXBeginEvent(commandList, 0, L"ShadowPass"));
	auto& rt = renderTargets_[rtIndex];
	rt.ResourceTransition(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	commandList->RSSetViewports(1, &rt.GetViewport());
	commandList->RSSetScissorRects(1, &rt.GetScissorRect());
	commandList->OMSetRenderTargets(0, nullptr, false, &rt.view.cpuHandle);
}

void Renderer::EndShadowPass(RTIndex rtIndex) {
	auto* commandList = commandList_.Get();
	auto& rt = renderTargets_[rtIndex];
	rt.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PIX(PIXEndEvent(commandList));
}
void Renderer::ShadowPass(const ShaderStructures::ShadowCmd& cmd) {
	auto* commandList = commandList_.Get();
	auto& state = pipelineStates_.states[cmd.shader];
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	commandList->SetPipelineState(state.pipelineState.Get());
	auto entry = frame_->desc.Push(1);
	frame_->BindCBV(entry.cpuHandle, cmd.mvp);

	assert(cmd.vb != InvalidBuffer);
	assert(cmd.ib != InvalidBuffer);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
		{ buffers_[cmd.vb].bufferLocation + cmd.submesh.vertexByteOffset,
		(UINT)buffers_[cmd.vb].size - cmd.submesh.vertexByteOffset,
		cmd.submesh.stride } };
	commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);

	D3D12_INDEX_BUFFER_VIEW	indexBufferView;
	const auto& buffer = buffers_[cmd.ib];
	indexBufferView.BufferLocation = buffer.bufferLocation + cmd.submesh.indexByteOffset;
	indexBufferView.SizeInBytes = (UINT)buffer.size - cmd.submesh.indexByteOffset;
	indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	commandList->IASetIndexBuffer(&indexBufferView);
	
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawIndexedInstanced(cmd.submesh.count, 1, 0, 0, 0);
}
void Renderer::DownsampleDepth(ID3D12GraphicsCommandList* commandList) {
	halfResDepthRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	depthStencil_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	Setup(commandList, Downsample, halfResDepthRT_, L"DownsampleDepth");

	auto entry = frame_->desc.Push(2);
	frame_->BindCBV(entry.cpuHandle, (float)depthStencil_.width / halfResDepthRT_.width);
	frame_->BindSRV(entry.cpuHandle, depthStencil_.resources->Get());
	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	commandList->SetGraphicsRootDescriptorTable(0, entry.gpuHandle);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->DrawInstanced(4, 1, 0, 0);
	halfResDepthRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PIX(PIXEndEvent(commandList));
}
template<int RTCount> 
void Renderer::Setup(ID3D12GraphicsCommandList* commandList, ShaderId shaderId, const RT<RTCount>& rt, PCWSTR eventName) {
	PIX(PIXBeginEvent(commandList, 0, eventName));
	auto& state = pipelineStates_.states[shaderId];
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	commandList->SetPipelineState(state.pipelineState.Get());
	commandList->RSSetViewports(1, &rt.GetViewport());
	commandList->RSSetScissorRects(1, &rt.GetScissorRect());
	commandList->OMSetRenderTargets(RTCount, &rt.view.cpuHandle, true, nullptr);
}
void Renderer::SSAOBlurPass(ID3D12GraphicsCommandList* commandList) {
	Setup(deferredCommandList_.Get(), Blur4x4R32, ssaoRT_, L"SSAOBlur4x4");
	ssaoRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	ssaoBlurRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	auto entry = frame_->desc.Push(1);
	frame_->BindSRV(entry.cpuHandle, ssaoRT_.resources->Get());
	commandList->SetGraphicsRootDescriptorTable(0, entry.gpuHandle);
	commandList->OMSetRenderTargets(1, &ssaoBlurRT_.view.cpuHandle, false, nullptr);
	commandList->DrawInstanced(4, 1, 0, 0);
	ssaoBlurRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PIX(PIXEndEvent(commandList));
}
void Renderer::SSAOPass(const SSAOCmd& cmd) {
	ID3D12GraphicsCommandList* commandList = deferredCommandList_.Get();
	DownsampleDepth(commandList);
	auto* depth = &halfResDepthRT_;
	depth->ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	gbuffersRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // normal rt needed
	ssaoRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Setup(commandList, SSAOShader, ssaoRT_, L"SSAOPass");
	auto entry = frame_->desc.Push(7);
	frame_->BindCBV(entry.cpuHandle, cmd.ip);
	frame_->BindCBV(entry.cpuHandle, cmd.scene);
	frame_->BindCBV(entry.cpuHandle, cmd.ao);
	frame_->BindCBV(entry.cpuHandle, ssao_.kernelResource->GetGPUVirtualAddress(), (UINT)ssao_.size256);
	frame_->BindSRV(entry.cpuHandle, depth->resources[0].Get());
	frame_->BindSRV(entry.cpuHandle, gbuffersRT_.resources[(int)PipelineStates::RTTs::NormalVS].Get());
	frame_->BindSRV(entry.cpuHandle, buffers_[cmd.random].resource.Get());

	commandList->SetGraphicsRootDescriptorTable(0, entry.gpuHandle);
	entry.gpuHandle.Offset(1, frame_->desc.GetDescriptorSize());
	commandList->SetGraphicsRootDescriptorTable(1, entry.gpuHandle);

	commandList->DrawInstanced(4, 1, 0, 0);
	ssaoRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	SSAOBlurPass(commandList);
	PIX(PIXEndEvent(commandList));
}
void Renderer::DoLightingPass(const ShaderStructures::DeferredCmd& cmd) {
	if (!loadingComplete_) return;
	ID3D12GraphicsCommandList* commandList = deferredCommandList_.Get();

	PIX(PIXBeginEvent(commandList, 0, L"DoLightingPass"));
	gbuffersRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	ssaoRT_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	depthStencil_.ResourceTransition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->RSSetViewports(1, &backBufferRTs_.GetViewport());
	commandList->RSSetScissorRects(1, &backBufferRTs_.GetScissorRect());
	auto& state = pipelineStates_.states[DeferredPBR];
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	commandList->SetPipelineState(state.pipelineState.Get());
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetBackBufferView();
	commandList->OMSetRenderTargets(1, &rtv, false, nullptr);

	const int descSize = frame_->desc.GetDescriptorSize();
#ifdef DEBUG_RT
	#define BINDING_COUNT 10
#else
	#define BINDING_COUNT 9
#endif
	auto entry = frame_->desc.Push(1 + BINDING_COUNT);
	// Scene
	frame_->BindCBV(entry.cpuHandle, cmd.scene);
	// RTs
	
	frame_->BindSRV(entry.cpuHandle, gbuffersRT_.resources[(int)PipelineStates::RTTs::Albedo].Get());
	frame_->BindSRV(entry.cpuHandle, gbuffersRT_.resources[(int)PipelineStates::RTTs::NormalWS].Get());
	frame_->BindSRV(entry.cpuHandle, gbuffersRT_.resources[(int)PipelineStates::RTTs::Material].Get());
#ifdef DEBUG_RT
	frame_->BindSRV(entry.cpuHandle, gbuffersRT_.resources[(int)PipelineStates::RTTs::Debug].Get());
#endif
	// Depth
	frame_->BindSRV(entry.cpuHandle, depthStencil_.resources->Get());
	// Irradiance
	frame_->BindCubeSRV(entry.cpuHandle, buffers_[cmd.irradiance].resource.Get());
	// Prefiltered env. map
	frame_->BindCubeSRV(entry.cpuHandle, buffers_[cmd.prefilteredEnvMap].resource.Get());
	// BRDFLUT
	frame_->BindSRV(entry.cpuHandle, buffers_[cmd.BRDFLUT].resource.Get());
	// SSAO
	frame_->BindSRV(entry.cpuHandle, ssaoBlurRT_.resources->Get());
	// SSAO debug
	frame_->BindSRV(entry.cpuHandle, ssaoRT_.resources[1].Get());

	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	commandList->SetGraphicsRootDescriptorTable(0, entry.gpuHandle);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->DrawInstanced(4, 1, 0, 0);

	PIXEndEvent(commandList);
}

std::array<glm::vec4, Renderer::SSAO::kKernelSize> Renderer::SSAO::GenKernel() {
	std::array<glm::vec4, Renderer::SSAO::kKernelSize> result;
	std::uniform_real_distribution dist(-1.f, 1.f);
	// http://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html
	for (int i = 0; i < kKernelSize; ++i) {
		glm::vec4 v = { dist(mt), dist(mt), (dist(mt)/* * .5f + .5f*/) /*0..1*/, 0.f/*pad*/};
		v = glm::normalize(v);
		float scale = (dist(mt) * .5f + .5f) * .75f + .25f; /* 0.25..1*/
		v = glm::mix(glm::vec4{}, v, scale * scale); 
		result[i] = v;
	}
	return result;
}