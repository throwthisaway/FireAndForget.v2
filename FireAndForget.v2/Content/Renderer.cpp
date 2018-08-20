#include "pch.h"
#include "Renderer.h"
#include "..\Common\DirectXHelper.h"
#include "..\source\cpp\Assets.hpp"
// TODO::
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

void RendererWrapper::Init(Renderer* renderer) {
	this->renderer = renderer;
}

void RendererWrapper::BeginUploadResources() {
	renderer->BeginUploadResources();
}

BufferIndex RendererWrapper::CreateBuffer(const void* buffer, size_t sizeInBytes, size_t elementSize) {
	return renderer->CreateBuffer(buffer, sizeInBytes, elementSize);
}

namespace {
	DXGI_FORMAT PixelFormatToDXGIFormat(Img::PixelFormat format) {
		switch (format) {
			case Img::PixelFormat::RGBA8:
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			case Img::PixelFormat::BGRA8:
				return DXGI_FORMAT_B8G8R8A8_UNORM;
			default:
				assert(false);
		}
		return DXGI_FORMAT_UNKNOWN;
	}
}
BufferIndex RendererWrapper::CreateTexture(const void* buffer, uint64_t width, uint32_t height, uint8_t bytesPerPixel, Img::PixelFormat format) {
	return renderer->CreateTexture(buffer, width, height, bytesPerPixel, PixelFormatToDXGIFormat(format));
}
void RendererWrapper::EndUploadResources() {
	renderer->EndUploadResources();
}

ShaderResourceIndex RendererWrapper::CreateShaderResource(uint32_t size, uint16_t count) {
	return renderer->CreateShaderResource(size, count);
}
void RendererWrapper::UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size) {
	return renderer->UpdateShaderResource(shaderResourceIndex, data, size);
}

DescAllocEntryIndex RendererWrapper::AllocDescriptors(uint16_t count) {
	return renderer->AllocDescriptors(count);
}
void RendererWrapper::CreateCBV(DescAllocEntryIndex index, uint16_t offset, ShaderResourceIndex resourceIndex) {
	renderer->CreateCBV(index, offset, resourceIndex);
}
void RendererWrapper::CreateSRV(DescAllocEntryIndex index, uint16_t offset, BufferIndex textureBufferIndex) {
	renderer->CreateSRV(index, offset, textureBufferIndex);
}
void RendererWrapper::CreateCBV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, ShaderResourceIndex resourceIndex) {
	renderer->CreateCBV(index, offset, frame, resourceIndex);
}
void RendererWrapper::CreateSRV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, BufferIndex textureBufferIndex) {
	renderer->CreateSRV(index, offset, frame, textureBufferIndex);
}

void RendererWrapper::BeginRender() {
	renderer->BeginRender();
}
size_t RendererWrapper::StartRenderPass() {
	return renderer->StartRenderPass();
}
uint32_t RendererWrapper::GetCurrenFrameIndex() {
	return renderer->m_deviceResources->GetCurrentFrameIndex();
}

void RendererWrapper::SetDeferredBuffers(const ShaderStructures::DeferredBuffers& deferredBuffers) {
	renderer->SetDeferredBuffers(deferredBuffers);
}
//
//ResourceHeapHandle RendererWrapper::GetStaticShaderResourceHeap(unsigned short descCountNeeded) {
//	return renderer->GetStaticShaderResourceHeap(descCountNeeded);
//}
//ShaderResourceIndex RendererWrapper::GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, size_t size, unsigned short count) {
//	return renderer->GetShaderResourceIndex(shaderResourceHeap, size, count);
//}
//void RendererWrapper::UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size) {
//	renderer->UpdateShaderResource(shaderResourceIndex, data, size);
//}
//ShaderResourceIndex RendererWrapper::GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, BufferIndex textureIndex) {
//	return renderer->GetShaderResourceIndex(shaderResourceHeap, textureIndex);
//}

using namespace Microsoft::WRL;
using namespace DirectX;
using namespace Windows::Foundation;

namespace {
	static constexpr size_t defaultDescCount = 256, defaultBufferSize = 65536, defaultCBFrameAllocSize = 16384, defaultDescFrameAllocCount = 16;
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
	bufferUpload_.intermediateResources.clear();
}
BufferIndex Renderer::CreateTexture(const void* buffer, UINT64 width, UINT height, UINT bytesPerPixel, DXGI_FORMAT format) {
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height);

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
	NAME_D3D12_OBJECT(resource);
	{
		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = buffer;
		data.RowPitch = width * bytesPerPixel;
		data.SlicePitch = data.RowPitch * height;

		UpdateSubresources(bufferUpload_.cmdList.Get(), resource.Get(), bufferUpload.Get(), 0, 0, 1, &data);

		CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		bufferUpload_.cmdList->ResourceBarrier(1, &vertexBufferResourceBarrier);
	}
	buffers_.push_back({ resource, 0, 0, 0, format });
	return (BufferIndex)buffers_.size() - 1;
}
BufferIndex Renderer::CreateBuffer(const void* buffer, size_t sizeInBytes, size_t elementSize) {
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
	buffers_.push_back({ resource, resource->GetGPUVirtualAddress(), sizeInBytes, elementSize});
	return (BufferIndex)buffers_.size() - 1;
}

void Renderer::CreateDeviceDependentResources() {
	cbAlloc_.Init(m_deviceResources->GetD3DDevice(), defaultBufferSize);
	for (int i = 0; i < _countof(cbFrameAlloc_); ++i)
		cbFrameAlloc_[i].Init(m_deviceResources->GetD3DDevice(), defaultCBFrameAllocSize);
	for (int i = 0; i < _countof(stagingDescriptorFrameAlloc_); ++i)
		stagingDescriptorFrameAlloc_[i].Init(m_deviceResources->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, defaultDescFrameAllocCount, true);
	submitDescriptorFrameAlloc_.Init(m_deviceResources->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, defaultDescFrameAllocCount, false);

	for (UINT i = 0; i < DX::c_frameCount; ++i) {
		descAlloc_[i].Init(m_deviceResources->GetD3DDevice(), defaultDescCount);
	}
	pipelineStates_.CreateDeviceDependentResources();
	auto d3dDevice = m_deviceResources->GetD3DDevice();

	// Buffer upload...
	DX::ThrowIfFailed(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&bufferUpload_.cmdAllocator)));
	DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, bufferUpload_.cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&bufferUpload_.cmdList)));
	bufferUpload_.cmdList->Close();
	NAME_D3D12_OBJECT(bufferUpload_.cmdList);
	// Buffer upload...

	pipelineStates_.completionTask_.then([this]() {
		loadingComplete_ = true;
	});

	for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
		if (pipelineStates_.states_[i].deferred) continue;
		ID3D12CommandAllocator* firstCommandAllocator = nullptr;
		for (UINT n = 0; n < FrameCount; n++) {
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
			DX::ThrowIfFailed(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
			commandAllocators_.push_back(commandAllocator);
			if (!n) firstCommandAllocator = commandAllocator.Get();
		}
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, firstCommandAllocator, nullptr, IID_PPV_ARGS(&commandList)));
		commandList->Close();
		NAME_D3D12_OBJECT(commandList);
		commandLists_.push_back(commandList);
	}
	for (UINT n = 0; n < FrameCount; n++)
		DX::ThrowIfFailed(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&deferredCommandAllocators_[n])));
	DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, deferredCommandAllocators_[0].Get(), nullptr, IID_PPV_ARGS(&deferredCommandList_)));
	deferredCommandList_->Close();
	NAME_D3D12_OBJECT(deferredCommandList_);

	// Create descriptor heaps for render target views
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = ShaderStructures::RenderTargetCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_)));
	NAME_D3D12_OBJECT(rtvHeap_);
	rtvDescriptorSize_ = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	BeginUploadResources();
	// fullscreen quad...
	struct VertexPUV {
		DirectX::XMFLOAT2 pos, uv;
	};
	VertexPUV quad[] = { { { -1., -1.f },{ 0.f, 1.f } },{ { -1., 1.f },{ 0.f, 0.f } },{ { 1., -1.f },{ 1.f, 1.f } },{ { 1., 1.f },{ 1.f, 0.f } } };
	fsQuad_ = CreateBuffer(quad, sizeof(quad), sizeof(VertexPUV));
	EndUploadResources();
}

void Renderer::CreateWindowSizeDependentResources() {
	Size outputSize = m_deviceResources->GetOutputSize();
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	m_scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(rtvHeap_->GetCPUDescriptorHandleForHeapStart());
	uint16_t offset = 0;
	D3D12_CLEAR_VALUE clearValue;
	memcpy(clearValue.Color, Black, sizeof(clearValue.Color));
	for (int j = 0; j < ShaderStructures::RenderTargetCount; ++j) {
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(PipelineStates::renderTargetFormats[j], (UINT)viewport.Width, (UINT)viewport.Height);
		clearValue.Format = PipelineStates::renderTargetFormats[j];
		bufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		auto device = m_deviceResources->GetD3DDevice();
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		DX::ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,// StartRenderPass sets up the proper state D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clearValue,
			IID_PPV_ARGS(&resource)));
		rtt_[j] = resource;
		WCHAR name[25];
		if (swprintf_s(name, L"renderTarget[%u]", j) > 0) DX::SetName(resource.Get(), name);

		D3D12_RENDER_TARGET_VIEW_DESC desc = {};
		desc.Format = PipelineStates::renderTargetFormats[j];
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		device->CreateRenderTargetView(rtt_[j].Get(), &desc, rtvDescriptor);
		rtvDescriptor.Offset(rtvDescriptorSize_);
	}
}

void Renderer::Update(DX::StepTimer const& timer) {
	if (loadingComplete_){
	}
}

void Renderer::BeginRender() {
	if (!loadingComplete_) return;

	cbFrameAlloc_[m_deviceResources->GetCurrentFrameIndex()].Reset();
	submitDescriptorFrameAlloc_.Reset();
	stagingDescriptorFrameAlloc_[m_deviceResources->GetCurrentFrameIndex()].Reset();
	m_deviceResources->GetCommandAllocator()->Reset();
	size_t i = 0;
	for (auto& commandList : commandLists_) {
		auto* commandAllocator = commandAllocators_[m_deviceResources->GetCurrentFrameIndex() + i * FrameCount].Get();
		DX::ThrowIfFailed(commandAllocator->Reset());
		DX::ThrowIfFailed(commandList->Reset(commandAllocator, nullptr));
		++i;
	}
}
size_t Renderer::StartRenderPass() {
	if (!loadingComplete_) return 0;
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	// Indicate this resource will be in use as a render target.
	CD3DX12_RESOURCE_BARRIER renderTargetResourceBarriers[ShaderStructures::RenderTargetCount + 1];
	for (int i = 0; i < RenderTargetCount; ++i) {
		renderTargetResourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(rtt_[i].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	renderTargetResourceBarriers[RenderTargetCount] = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetDepthStencil(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	//D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_deviceResources->GetRenderTargetView();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_deviceResources->GetDepthStencilView();

	// TODO:: calculate in CreateWindowSizeDependentResources...
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvs[RenderTargetCount];
	for (int i = 0; i < RenderTargetCount; ++i) {
		rtvs[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), i, rtvDescriptorSize_);
	}
	// assign pipelinestates with command lists
	for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
		auto& state = pipelineStates_.states_[i];
		if (state.deferred) {

		}
		else {
			auto* commandList = commandLists_[i].Get();
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &m_scissorRect);
			if (i == 0) {
				commandList->ResourceBarrier(_countof(renderTargetResourceBarriers), renderTargetResourceBarriers);
				for (int j = 0; j < RenderTargetCount; ++j) {
					commandList->ClearRenderTargetView(rtvs[j], Black, 0, nullptr);
				}
				commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			}
			commandList->OMSetRenderTargets(RenderTargetCount, rtvs, false, &depthStencilView);
			commandList->SetPipelineState(state.pipelineState.Get());
			// Set the graphics root signature and descriptor heaps to be used by this frame.
			commandList->SetGraphicsRootSignature(pipelineStates_.rootSignatures_[state.rootSignatureId].Get());
		}
	}
	return 0;
}

template<>
void Renderer::Submit(const ShaderStructures::DebugCmd& cmd) {
	if (!loadingComplete_) return;
	const auto id = ShaderStructures::Debug;
	auto& state = pipelineStates_.states_[id];
	auto* commandList = commandLists_[id].Get();
	PIXBeginEvent(commandList, 0, L"SubmitDebugCmd");
	{
		const auto& desc = descAlloc_[m_deviceResources->GetCurrentFrameIndex()].Get(cmd.descAllocEntryIndex);
		ID3D12DescriptorHeap* ppHeaps[] = { desc.heap };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		// Bind the current frame's constant buffer to the pipeline.
		auto d3dDevice = m_deviceResources->GetD3DDevice();
		for (auto binding : cmd.bindings) {
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = descAlloc_[m_deviceResources->GetCurrentFrameIndex()].GetGPUHandle(cmd.descAllocEntryIndex, binding.offset);
			commandList->SetGraphicsRootDescriptorTable(binding.paramIndex, gpuHandle);
		}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		assert(cmd.vb != InvalidBuffer);
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		{
			const auto& buffer = buffers_[cmd.vb];
			vertexBufferView.BufferLocation = buffer.bufferLocation;
			vertexBufferView.StrideInBytes = (UINT)buffer.elementSize;
			vertexBufferView.SizeInBytes = (UINT)buffer.size;
		}
		// TODO:: nb and uvb
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		if (cmd.ib != InvalidBuffer) {
			D3D12_INDEX_BUFFER_VIEW	indexBufferView;
			{
				const auto& buffer = buffers_[cmd.ib];
				indexBufferView.BufferLocation = buffer.bufferLocation;
				indexBufferView.SizeInBytes = (UINT)buffer.size;
				indexBufferView.Format = DXGI_FORMAT_R16_UINT;
			}
			commandList->IASetIndexBuffer(&indexBufferView);
			commandList->DrawIndexedInstanced(cmd.count, 1, cmd.offset, 0/* reorder vertices to support uint16_t indexing*/, 0);
		}
		else {
			commandList->DrawInstanced(cmd.count, 1, cmd.offset, 0);
		}
	}
	PIXEndEvent(commandList);
}

template<>
void Renderer::Submit(const ShaderStructures::PosCmd& cmd) {
	if (!loadingComplete_) return;
	const auto id = ShaderStructures::Pos;
	auto& state = pipelineStates_.states_[id];
	auto* commandList = commandLists_[id].Get();
	PIXBeginEvent(commandList, 0, L"SubmitPosCmd");
	{
		const auto& desc = descAlloc_[m_deviceResources->GetCurrentFrameIndex()].Get(cmd.descAllocEntryIndex);
		ID3D12DescriptorHeap* ppHeaps[] = { desc.heap };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		// Bind the current frame's constant buffer to the pipeline.
		auto d3dDevice = m_deviceResources->GetD3DDevice();
		for (auto binding : cmd.bindings) {
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = descAlloc_[m_deviceResources->GetCurrentFrameIndex()].GetGPUHandle(cmd.descAllocEntryIndex, binding.offset);
			commandList->SetGraphicsRootDescriptorTable(binding.paramIndex, gpuHandle);
		}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		assert(cmd.vb != InvalidBuffer);
		assert(cmd.nb != InvalidBuffer);
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
			{ buffers_[cmd.vb].bufferLocation,
			(UINT)buffers_[cmd.vb].size,
			(UINT)buffers_[cmd.vb].elementSize },

			{ buffers_[cmd.nb].bufferLocation,
			(UINT)buffers_[cmd.nb].size,
			(UINT)buffers_[cmd.nb].elementSize } };

		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
		if (cmd.ib != InvalidBuffer) {
			D3D12_INDEX_BUFFER_VIEW	indexBufferView;
			{
				const auto& buffer = buffers_[cmd.ib];
				indexBufferView.BufferLocation = buffer.bufferLocation;
				indexBufferView.SizeInBytes = (UINT)buffer.size;
				indexBufferView.Format = DXGI_FORMAT_R16_UINT;
			}
			commandList->IASetIndexBuffer(&indexBufferView);
			commandList->DrawIndexedInstanced(cmd.count, 1, cmd.offset, 0/* reorder vertices to support uint16_t indexing*/, 0);
		}
		else {
			commandList->DrawInstanced(cmd.count, 1, cmd.offset, 0);
		}
	}
	PIXEndEvent(commandList);
}

template<>
void Renderer::Submit(const ShaderStructures::TexCmd& cmd) {
	if (!loadingComplete_) return;
	const auto id = ShaderStructures::Tex;
	auto& state = pipelineStates_.states_[id];
	auto* commandList = commandLists_[id].Get();
	PIXBeginEvent(commandList, 0, L"SubmitTexCmd");
	{
		const auto& desc = descAlloc_[m_deviceResources->GetCurrentFrameIndex()].Get(cmd.descAllocEntryIndex);
		ID3D12DescriptorHeap* ppHeaps[] = { desc.heap };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		// Bind the current frame's constant buffer to the pipeline.
		auto d3dDevice = m_deviceResources->GetD3DDevice();
		UINT cbvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto binding : cmd.bindings) {
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = descAlloc_[m_deviceResources->GetCurrentFrameIndex()].GetGPUHandle(cmd.descAllocEntryIndex, binding.offset);
			commandList->SetGraphicsRootDescriptorTable(binding.paramIndex, gpuHandle);
		}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		assert(cmd.vb != InvalidBuffer);
		assert(cmd.nb != InvalidBuffer);
		assert(cmd.uvb != InvalidBuffer);
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
			{ buffers_[cmd.vb].bufferLocation,
			(UINT)buffers_[cmd.vb].size,
			(UINT)buffers_[cmd.vb].elementSize },

			{ buffers_[cmd.nb].bufferLocation,
			(UINT)buffers_[cmd.nb].size,
			(UINT)buffers_[cmd.nb].elementSize },

			{ buffers_[cmd.uvb].bufferLocation,
			(UINT)buffers_[cmd.uvb].size,
			(UINT)buffers_[cmd.uvb].elementSize } };

		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);

		if (cmd.ib != InvalidBuffer) {
			D3D12_INDEX_BUFFER_VIEW	indexBufferView;
			{
				const auto& buffer = buffers_[cmd.ib];
				indexBufferView.BufferLocation = buffer.bufferLocation;
				indexBufferView.SizeInBytes = (UINT)buffer.size;
				indexBufferView.Format = DXGI_FORMAT_R16_UINT;
			}

			commandList->IASetIndexBuffer(&indexBufferView);
			commandList->DrawIndexedInstanced(cmd.count, 1, cmd.offset, 0/* reorder vertices to support uint16_t indexing*/, 0);
		} else {
			commandList->DrawInstanced(cmd.count, 1, cmd.offset, 0);
		}
	}
	PIXEndEvent(commandList);
}

bool Renderer::Render() {
	if (!loadingComplete_) return false;

	// TODO:: fixed size array
	std::vector<ID3D12CommandList*> ppCommandLists;

	for (auto& commandList : commandLists_) {
		commandList->Close();
		ppCommandLists.push_back(commandList.Get());
	}
	// TODO:: !!! why here works???
	uint16_t offset = 0;	// cScene
	++offset;
	for (int j = 0; j < ShaderStructures::RenderTargetCount; ++j)
		CreateSRV(deferredBuffers_.descAllocEntryIndex, offset + j, rtt_[j].Get(), PipelineStates::renderTargetFormats[j]);
	CreateSRV(deferredBuffers_.descAllocEntryIndex, offset + ShaderStructures::RenderTargetCount, m_deviceResources->GetDepthStencil(), DXGI_FORMAT_R32_FLOAT/*m_deviceResources->GetDepthBufferFormat()*/);
	// !!! TODO::

	CD3DX12_RESOURCE_BARRIER presentResourceBarriers[RenderTargetCount + 2];
	for (int i = 0; i < RenderTargetCount; ++i)
		presentResourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(rtt_[i].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	presentResourceBarriers[RenderTargetCount] = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetDepthStencil(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	presentResourceBarriers[RenderTargetCount + 1] = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	auto* deferredCommandAllocator = deferredCommandAllocators_[m_deviceResources->GetCurrentFrameIndex()].Get();
	DX::ThrowIfFailed(deferredCommandAllocator->Reset());
	DX::ThrowIfFailed(deferredCommandList_->Reset(deferredCommandAllocator, nullptr));

	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	deferredCommandList_->RSSetViewports(1, &viewport);
	deferredCommandList_->RSSetScissorRects(1, &m_scissorRect);
	deferredCommandList_->ResourceBarrier(_countof(presentResourceBarriers), presentResourceBarriers);
	auto& state = pipelineStates_.states_[DeferredPBR];
	deferredCommandList_->SetGraphicsRootSignature(pipelineStates_.rootSignatures_[state.rootSignatureId].Get());
	deferredCommandList_->SetPipelineState(state.pipelineState.Get());
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_deviceResources->GetRenderTargetView();
	deferredCommandList_->OMSetRenderTargets(1, &renderTargetView, false, nullptr);
	ID3D12GraphicsCommandList* commandList = deferredCommandList_.Get();
	PIXBeginEvent(commandList, 0, L"SubmitDeferredPBRCmd");
	{
		const auto& desc = descAlloc_[m_deviceResources->GetCurrentFrameIndex()].Get(deferredBuffers_.descAllocEntryIndex);
		ID3D12DescriptorHeap* ppHeaps[] = { desc.heap };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		// Bind the current frame's constant buffer to the pipeline.
		auto d3dDevice = m_deviceResources->GetD3DDevice();
		UINT cbvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto binding : deferredBuffers_.bindings) {
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = descAlloc_[m_deviceResources->GetCurrentFrameIndex()].GetGPUHandle(deferredBuffers_.descAllocEntryIndex, binding.offset);
			commandList->SetGraphicsRootDescriptorTable(binding.paramIndex, gpuHandle);
		}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		assert(fsQuad_ != InvalidBuffer);
	
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
			{ buffers_[fsQuad_].bufferLocation,
			(UINT)buffers_[fsQuad_].size,
			(UINT)buffers_[fsQuad_].elementSize } };

		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
		commandList->DrawInstanced(4, 1, 0, 0);
	}
	PIXEndEvent(commandList);

	// Indicate that the render target will now be used to present when the command list is done executing.
	CD3DX12_RESOURCE_BARRIER presentResourceBarrier =
		CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	deferredCommandList_->ResourceBarrier(1, &presentResourceBarrier);
	deferredCommandList_->Close();
	ppCommandLists.push_back(deferredCommandList_.Get());
	// Execute the command list.
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists((UINT)ppCommandLists.size(), &ppCommandLists.front());

	return true;
}

ShaderResourceIndex Renderer::CreateShaderResource(uint32_t size, uint16_t count) {
	return cbAlloc_.Push(size, count);
}
void Renderer::UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size) {
	memcpy(cbAlloc_.GetCPUAddress(shaderResourceIndex), data, size);
}
DescAllocEntryIndex Renderer::AllocDescriptors(uint16_t count) {
	DescAllocEntryIndex result;
	for (UINT i = 0; i < DX::c_frameCount; ++i) {
		result = descAlloc_[i].Push(count);
	}
	return result; // All of them should have the same index
}
void Renderer::CreateCBV(DescAllocEntryIndex index, uint16_t offset, ShaderResourceIndex resourceIndex) {
	for (UINT i = 0; i < DX::c_frameCount; ++i) {
		descAlloc_[i].CreateCBV(index, offset, cbAlloc_.GetGPUAddress(resourceIndex), cbAlloc_.GetSize(resourceIndex));
	}
}
void Renderer::CreateCBV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, ShaderResourceIndex resourceIndex) {
	descAlloc_[frame].CreateCBV(index, offset, cbAlloc_.GetGPUAddress(resourceIndex), cbAlloc_.GetSize(resourceIndex));
}
void Renderer::CreateSRV(DescAllocEntryIndex index, uint16_t offset, BufferIndex textureBufferIndex) {
	for (UINT i = 0; i < DX::c_frameCount; ++i) {
		descAlloc_[i].CreateSRV(index, offset, buffers_[textureBufferIndex].resource.Get(), buffers_[textureBufferIndex].format);
	}
}
void Renderer::CreateSRV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, BufferIndex textureBufferIndex) {
	CreateSRV(index, offset, frame, buffers_[textureBufferIndex].resource.Get(), buffers_[textureBufferIndex].format);
}
void Renderer::CreateSRV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, ID3D12Resource* resource, DXGI_FORMAT format) {
	descAlloc_[frame].CreateSRV(index, offset, resource, format);
}
void Renderer::CreateSRV(DescAllocEntryIndex index, uint16_t offset, ID3D12Resource* resource, DXGI_FORMAT format) {
	for (UINT i = 0; i < DX::c_frameCount; ++i)
		descAlloc_[i].CreateSRV(index, offset, resource, format);
}
// TODO:: these have to come after Renderer::Submit specialization
template<>
void RendererWrapper::Submit(const ShaderStructures::DebugCmd& cmd) {
	renderer->Submit(cmd);
}
template<>
void RendererWrapper::Submit(const ShaderStructures::PosCmd& cmd) {
	renderer->Submit(cmd);
}
template<>
void RendererWrapper::Submit(const ShaderStructures::TexCmd& cmd) {
	renderer->Submit(cmd);
}

void Renderer::SetDeferredBuffers(const ShaderStructures::DeferredBuffers& deferredBuffers) {
	deferredBuffers_ = deferredBuffers;
	uint16_t offset = 0;	// cScene
	++offset;
	for (int j = 0; j < ShaderStructures::RenderTargetCount; ++j)
		CreateSRV(deferredBuffers_.descAllocEntryIndex, offset + j, rtt_[j].Get(), PipelineStates::renderTargetFormats[j]);
	// TODO:: is one depthstencil enough???
	CreateSRV(deferredBuffers_.descAllocEntryIndex, offset + ShaderStructures::RenderTargetCount, m_deviceResources->GetDepthStencil(), DXGI_FORMAT_R32_FLOAT/*m_deviceResources->GetDepthBufferFormat()*/);
}
//ResourceHeapHandle Renderer::GetStaticShaderResourceHeap(unsigned short descCountNeeded) {
//	return shaderResources_.GetCurrentShaderResourceHeap(descCountNeeded);
//}
//size_t Renderer::GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, size_t size, unsigned short count) {
//	auto& resources = shaderResources_.GetShaderResourceHeap(shaderResourceHeap);
//	auto index = resources.PushPerFrameCBV(size, count);
//	size_t startIndex = shaderResourceDescriptors_.size();
//	for (unsigned short i = 0; i < count; ++i, ++index)
//		shaderResourceDescriptors_.emplace_back(&resources.Get(index));
//	return startIndex;
//}
//void Renderer::UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size) {
//	assert(shaderResourceDescriptors_.size() > shaderResourceIndex);
//	const StackAlloc::FrameDesc* frameDesc = shaderResourceDescriptors_[shaderResourceIndex];
//	memcpy(frameDesc->destination, data, size);
//}
//ShaderResourceIndex Renderer::GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, BufferIndex textureIndex) {
//	auto& resources = shaderResources_.GetShaderResourceHeap(shaderResourceHeap);
//	auto index = resources.PushSRV(buffers_[textureIndex].resource.Get(), buffers_[textureIndex].format);
//	shaderResourceDescriptors_.emplace_back(&resources.Get(index));
//	return shaderResourceDescriptors_.size() - 1;
//}

