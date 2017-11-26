#include "pch.h"
#include "Renderer.h"
#include "..\Common\DirectXHelper.h"
#include "..\source\cpp\Materials.h"
#include "..\source\cpp\Assets.hpp"

// Resource Binding Flow of Control - https://msdn.microsoft.com/en-us/library/windows/desktop/mt709154(v=vs.85).aspx
// Resource Binding - https://msdn.microsoft.com/en-us/library/windows/desktop/dn899206(v=vs.85).aspx
// Creating Descriptors - https://msdn.microsoft.com/en-us/library/windows/desktop/dn859358(v=vs.85).aspx
// https://www.braynzarsoft.net/viewtutorial/q16390-04-directx-12-braynzar-soft-tutorials
void RendererWrapper::Init(void* self) {
	this->self = self;
}

void RendererWrapper::BeginUploadResources() {
	reinterpret_cast<Renderer*>(self)->BeginUploadResources();
}

size_t RendererWrapper::CreateBuffer(const void* buffer, size_t length, size_t elementSize) {
	return reinterpret_cast<Renderer*>(self)->CreateBuffer(buffer, length, elementSize);
}

void RendererWrapper::EndUploadResources() {
	reinterpret_cast<Renderer*>(self)->EndUploadResources();
}

void RendererWrapper::BeginRender() {
	reinterpret_cast<Renderer*>(self)->BeginRender();
}
size_t RendererWrapper::StartRenderPass() {
	return reinterpret_cast<Renderer*>(self)->StartRenderPass();
}
void RendererWrapper::SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, const std::vector<size_t>& bufferIndices, const Mesh& model) {
	return reinterpret_cast<Renderer*>(self)->SubmitToEncoder(encoderIndex, pipelineIndex, bufferIndices, model);
}

ShaderResources& RendererWrapper::GetShaderResources() {
	return reinterpret_cast<Renderer*>(self)->shaderResources_;
}
uint32_t RendererWrapper::GetCurrenFrameIndex() {
	return reinterpret_cast<Renderer*>(self)->m_deviceResources->GetCurrentFrameIndex();
}

using namespace Microsoft::WRL;
using namespace DirectX;
using namespace Windows::Foundation;

Renderer::Renderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	pipelineStates_(deviceResources.get()),
	shaderResources_(deviceResources->GetD3DDevice()),
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
size_t Renderer::CreateBuffer(const void* buffer, size_t size, size_t elementSize) {
	auto d3dDevice = m_deviceResources->GetD3DDevice();
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	// Create the vertex buffer resource in the GPU's default heap and copy vertex data into it using the upload heap.
	// The upload resource must not be released until after the GPU has finished using it.
	Microsoft::WRL::ComPtr<ID3D12Resource> bufferUpload;

	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
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
		data.RowPitch = size;
		data.SlicePitch = data.RowPitch;

		UpdateSubresources(bufferUpload_.cmdList.Get(), resource.Get(), bufferUpload.Get(), 0, 0, 1, &data);

		CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		bufferUpload_.cmdList->ResourceBarrier(1, &vertexBufferResourceBarrier);
	}
	buffers_.push_back({ resource, resource->GetGPUVirtualAddress(), size, elementSize});
	return buffers_.size() - 1;
}

void Renderer::CreateDeviceDependentResources() {
	pipelineStates_.CreateDeviceDependentResources();
	auto d3dDevice = m_deviceResources->GetD3DDevice();
	// 1 command list for each pipeline state, and 1 for frame commands
	DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_deviceResources->GetCommandAllocator(), nullptr, IID_PPV_ARGS(&frameCommandList_)));
	frameCommandList_->Close();
	NAME_D3D12_OBJECT(frameCommandList_);

	// Buffer upload...
	DX::ThrowIfFailed(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&bufferUpload_.cmdAllocator)));
	DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, bufferUpload_.cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&bufferUpload_.cmdList)));
	bufferUpload_.cmdList->Close();
	NAME_D3D12_OBJECT(bufferUpload_.cmdList);
	// Buffer upload...

	pipelineStates_.completionTask_.then([this]() {
		auto d3dDevice = m_deviceResources->GetD3DDevice();
		for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
			DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_deviceResources->GetCommandAllocator(), nullptr, IID_PPV_ARGS(&commandList)));
			commandList->Close();
			NAME_D3D12_OBJECT(commandList);
			commandLists_.push_back(commandList);
		}
		for (UINT n = 0; n < DX::c_frameCount * commandLists_.size(); n++)
		{
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
			DX::ThrowIfFailed(
				d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator))
			);
			commandAllocators_.push_back(commandAllocator);
		}
		loadingComplete_ = true;
	});
}

void Renderer::CreateWindowSizeDependentResources() {
	Size outputSize = m_deviceResources->GetOutputSize();
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	m_scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
}

void Renderer::Update(DX::StepTimer const& timer) {
	if (loadingComplete_){
	}
}

void Renderer::BeginRender() {
	if (!loadingComplete_) return;
	DX::ThrowIfFailed(m_deviceResources->GetCommandAllocator()->Reset());
	DX::ThrowIfFailed(frameCommandList_->Reset(m_deviceResources->GetCommandAllocator(), nullptr));

	size_t i = 0;
	for (auto& commandList : commandLists_) {
		auto* commandAllocator = commandAllocators_[m_deviceResources->GetCurrentFrameIndex() + i * DX::c_frameCount].Get();
		DX::ThrowIfFailed(commandAllocator->Reset());
		DX::ThrowIfFailed(commandList->Reset(commandAllocator, nullptr));
		++i;
	}
}
size_t Renderer::StartRenderPass() {
	if (!loadingComplete_) return 0;
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	frameCommandList_->RSSetViewports(1, &viewport);
	frameCommandList_->RSSetScissorRects(1, &m_scissorRect);

	// Indicate this resource will be in use as a render target.
	CD3DX12_RESOURCE_BARRIER renderTargetResourceBarrier =
		CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	frameCommandList_->ResourceBarrier(1, &renderTargetResourceBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_deviceResources->GetRenderTargetView();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_deviceResources->GetDepthStencilView();
	frameCommandList_->ClearRenderTargetView(renderTargetView, DirectX::Colors::CornflowerBlue, 0, nullptr);
	frameCommandList_->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	frameCommandList_->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);

	// assign pipelinestates with command lists
	for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
		auto& state = pipelineStates_.states_[i];
		auto* commandList = commandLists_[i].Get();
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &m_scissorRect);
		commandList->ResourceBarrier(1, &renderTargetResourceBarrier);
		commandList->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
		commandList->SetPipelineState(state.pipelineState.Get());
		// Set the graphics root signature and descriptor heaps to be used by this frame.
		commandList->SetGraphicsRootSignature(pipelineStates_.rootSignatures_[state.rootSignatureId].Get());
	}
	return 0;
}

void Renderer::SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, const std::vector<size_t>& bufferIndices, const Mesh& model) {
	if (!loadingComplete_) return;

	auto& state = pipelineStates_.states_[pipelineIndex];
	auto* commandList = commandLists_[pipelineIndex].Get();
	PIXBeginEvent(commandList, 0, L"SubmitToEncoder");
	{
		ID3D12DescriptorHeap* ppHeaps[] = { shaderResources_.staticResources_.resource_.cbvHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		// Bind the current frame's constant buffer to the pipeline.
		size_t rootParameterIndex = 0;
		auto d3dDevice = m_deviceResources->GetD3DDevice();
		UINT cbvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (size_t index : bufferIndices) {
			auto& resource = shaderResources_.staticResources_.Get(index);
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = resource.gpuHandle;
			gpuHandle.Offset(m_deviceResources->GetCurrentFrameIndex(), cbvDescriptorSize);
			commandList->SetGraphicsRootDescriptorTable(rootParameterIndex++, gpuHandle);
		}
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		{
			const auto& buffer = buffers_[model.vb];
			vertexBufferView.BufferLocation = buffer.bufferLocation;
			vertexBufferView.StrideInBytes = (UINT)buffer.elementSize;
			vertexBufferView.SizeInBytes = (UINT)buffer.size;
		}
		D3D12_INDEX_BUFFER_VIEW	indexBufferView;
		{
			const auto& buffer = buffers_[model.ib];
			indexBufferView.BufferLocation = buffer.bufferLocation;
			indexBufferView.SizeInBytes = (UINT)buffer.size;
			indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		}
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&indexBufferView);
		for (auto layer : model.layers)
			for (auto submesh : layer.submeshes)
				commandList->DrawIndexedInstanced(submesh.count, 1, submesh.offset, 0/* reorder vertices to support uint16_t indexing*/, 0);
	}
	PIXEndEvent(commandList);
}
bool Renderer::Render() {
	if (!loadingComplete_) return false;

	std::vector<ID3D12CommandList*> ppCommandLists;
	// Indicate that the render target will now be used to present when the command list is done executing.
	CD3DX12_RESOURCE_BARRIER presentResourceBarrier =
		CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	frameCommandList_->ResourceBarrier(1, &presentResourceBarrier);
	DX::ThrowIfFailed(frameCommandList_->Close());
	ppCommandLists.push_back(frameCommandList_.Get());
	
	for (auto& commandList : commandLists_) {
		commandList->ResourceBarrier(1, &presentResourceBarrier);
		commandList->Close();
		ppCommandLists.push_back(commandList.Get());
	}

	// Execute the command list.
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(ppCommandLists.size(), &ppCommandLists.front());

	return true;
}
