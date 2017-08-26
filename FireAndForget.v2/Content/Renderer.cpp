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
void RendererWrapper::SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, const Materials::cBuffers& uniforms, const Model& model) {
	return reinterpret_cast<Renderer*>(self)->SubmitToEncoder(encoderIndex, pipelineIndex, uniforms, model);
}


using namespace Microsoft::WRL;
using namespace DirectX;
using namespace Windows::Foundation;

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
	DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_deviceResources->GetCommandAllocator(), nullptr, IID_PPV_ARGS(&commandList_)));
	commandList_->Close();
	NAME_D3D12_OBJECT(commandList_);

	// Buffer upload...
	DX::ThrowIfFailed(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&bufferUpload_.cmdAllocator)));
	DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, bufferUpload_.cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&bufferUpload_.cmdList)));
	bufferUpload_.cmdList->Close();
	NAME_D3D12_OBJECT(bufferUpload_.cmdList);
	// Buffer upload...

	pipelineStates_.completionTask_.then([this]() {
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
	DX::ThrowIfFailed(commandList_->Reset(m_deviceResources->GetCommandAllocator(), nullptr));
}
size_t Renderer::StartRenderPass() {
	if (!loadingComplete_) return 0;
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	commandList_->RSSetViewports(1, &viewport);
	commandList_->RSSetScissorRects(1, &m_scissorRect);

	// Indicate this resource will be in use as a render target.
	CD3DX12_RESOURCE_BARRIER renderTargetResourceBarrier =
		CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList_->ResourceBarrier(1, &renderTargetResourceBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_deviceResources->GetRenderTargetView();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_deviceResources->GetDepthStencilView();
	commandList_->ClearRenderTargetView(renderTargetView, DirectX::Colors::CornflowerBlue, 0, nullptr);
	commandList_->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList_->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
	return 0;
}

void Renderer::SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, const Materials::cBuffers& uniforms, const Model& model) {
	if (!loadingComplete_) return;	
	auto& state = pipelineStates_.states_[pipelineIndex];
	commandList_->SetPipelineState(state.pipelineState.Get());

	PIXBeginEvent(commandList_.Get(), 0, L"Draw the cube");
	{
		ID3D12DescriptorHeap* ppHeaps[] = { state.cbvHeap.Get() };
		size_t i = 0;
		for (auto& cbuffer : state.cbuffers) {
			// Update the constant buffer resource.
			UINT8* mappedConstantBuffer = cbuffer.Map();
			UINT8* destination = mappedConstantBuffer + (m_deviceResources->GetCurrentFrameIndex() * cbuffer.alignedConstantBufferSize);
			memcpy(destination, uniforms.data[i].first, uniforms.data[i].second/*cbuffer.dataSize*/);
			cbuffer.Unmap();
			++i;
		}
		// Set the graphics root signature and descriptor heaps to be used by this frame.
		commandList_->SetGraphicsRootSignature(pipelineStates_.rootSignatures_[state.rootSignatureId].Get());
		commandList_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		// Bind the current frame's constant buffer to the pipeline.
		size_t rootParameterIndex = 0;
		auto d3dDevice = m_deviceResources->GetD3DDevice();
		UINT cbvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(state.cbvHeap->GetGPUDescriptorHandleForHeapStart(), m_deviceResources->GetCurrentFrameIndex(), cbvDescriptorSize * state.cbuffers.size());
		for (auto& cbuffer : state.cbuffers) {
			commandList_->SetGraphicsRootDescriptorTable(rootParameterIndex++, gpuHandle);
			gpuHandle.Offset(cbvDescriptorSize);
		}
		commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		{
			const auto& buffer = buffers_[model.vertices];
			vertexBufferView.BufferLocation = buffer.bufferLocation;
			vertexBufferView.StrideInBytes = (UINT)buffer.elementSize;
			vertexBufferView.SizeInBytes = (UINT)buffer.size;
		}
		D3D12_INDEX_BUFFER_VIEW	indexBufferView;
		{
			const auto& buffer = buffers_[model.index];
			indexBufferView.BufferLocation = buffer.bufferLocation;
			indexBufferView.SizeInBytes = (UINT)buffer.size;
			indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		}
		commandList_->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList_->IASetIndexBuffer(&indexBufferView);
		commandList_->DrawIndexedInstanced(model.count, 1, 0, 0, 0);
	}
	PIXEndEvent(commandList_.Get());
}
bool Renderer::Render() {
	if (!loadingComplete_) return false;

	// Indicate that the render target will now be used to present when the command list is done executing.
	CD3DX12_RESOURCE_BARRIER presentResourceBarrier =
		CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	commandList_->ResourceBarrier(1, &presentResourceBarrier);
	DX::ThrowIfFailed(commandList_->Close());

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { commandList_.Get() };
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	return true;
}
