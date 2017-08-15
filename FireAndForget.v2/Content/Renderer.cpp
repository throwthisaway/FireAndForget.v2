#include "pch.h"
#include "Renderer.h"
#include "..\Common\DirectXHelper.h"
#include "..\source\cpp\Materials.h"
#include "..\source\cpp\Assets.hpp"

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
void RendererWrapper::SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, uint8_t* uniforms, const Model& model) {
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
	DX::ThrowIfFailed(m_deviceResources->GetCommandAllocator()->Reset());
	DX::ThrowIfFailed(commandList_->Reset(m_deviceResources->GetCommandAllocator(), nullptr));
}
void Renderer::EndUploadResources() {
	DX::ThrowIfFailed(commandList_->Close());
	ID3D12CommandList* ppCommandLists[] = { commandList_.Get() };
	m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	m_deviceResources->WaitForGpu();
	bufferUploads_.clear();
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
	bufferUploads_.push_back(bufferUpload);
	NAME_D3D12_OBJECT(resource);
	{
		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = buffer;
		data.RowPitch = size;
		data.SlicePitch = data.RowPitch;

		UpdateSubresources(commandList_.Get(), resource.Get(), bufferUpload.Get(), 0, 0, 1, &data);

		CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		commandList_->ResourceBarrier(1, &vertexBufferResourceBarrier);
	}
	buffers_.push_back({ resource, resource->GetGPUVirtualAddress(), size, elementSize});
	return buffers_.size() - 1;
}
Renderer::CBuffer Renderer::CreateAndMapConstantBuffers(size_t size) {
	auto d3dDevice = m_deviceResources->GetD3DDevice();
	// Constant buffers must be 256-byte aligned.
	const UINT c_alignedConstantBufferSize = (size + 255) & ~255;
	ComPtr<ID3D12DescriptorHeap>  cbvHeap;
	// Create a descriptor heap for the constant buffers.
	{

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = DX::c_frameCount;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbvHeap)));

		NAME_D3D12_OBJECT(cbvHeap);
	}
	ComPtr<ID3D12Resource> constantBuffer;
	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(DX::c_frameCount * c_alignedConstantBufferSize);
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&constantBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constantBuffer)));

	NAME_D3D12_OBJECT(constantBuffer);

	// Create constant buffer views to access the upload buffer.
	D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = constantBuffer->GetGPUVirtualAddress();
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart());
	UINT cbvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int n = 0; n < DX::c_frameCount; n++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = cbvGpuAddress;
		desc.SizeInBytes = c_alignedConstantBufferSize;
		d3dDevice->CreateConstantBufferView(&desc, cbvCpuHandle);

		cbvGpuAddress += desc.SizeInBytes;
		cbvCpuHandle.Offset(cbvDescriptorSize);
	}

	// Map the constant buffers.
	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	UINT8* mappedConstantBuffer = nullptr;
	DX::ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedConstantBuffer)));
	ZeroMemory(mappedConstantBuffer, DX::c_frameCount * c_alignedConstantBufferSize);
	return { cbvHeap, constantBuffer, mappedConstantBuffer, cbvDescriptorSize, c_alignedConstantBufferSize, size};
}
void Renderer::CreateDeviceDependentResources() {
	pipelineStates_.CreateDeviceDependentResources();
	auto d3dDevice = m_deviceResources->GetD3DDevice();
	DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_deviceResources->GetCommandAllocator(), nullptr, IID_PPV_ARGS(&commandList_)));
	commandList_->Close();
	NAME_D3D12_OBJECT(commandList_);

	// Material::ColPos
	cbuffers_.push_back(CreateAndMapConstantBuffers(sizeof(ModelViewProjectionConstantBuffer)));

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

void Renderer::SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, uint8_t* uniforms, const Model& model) {
	if (!loadingComplete_) return;	
	const auto& state = pipelineStates_.states_[pipelineIndex];
	commandList_->SetPipelineState(state.pipelineState.Get());

	PIXBeginEvent(commandList_.Get(), 0, L"Draw the cube");
	{
		auto& cbuffer = cbuffers_[pipelineIndex];
		// Update the constant buffer resource.
		UINT8* destination = cbuffer.mappedConstantBuffer + (m_deviceResources->GetCurrentFrameIndex() * cbuffer.alignedConstantBufferSize);
		memcpy(destination, uniforms, cbuffer.dataSize);

		// Set the graphics root signature and descriptor heaps to be used by this frame.
		commandList_->SetGraphicsRootSignature(pipelineStates_.rootSignatures_[state.rootSignatureId].Get());
		ID3D12DescriptorHeap* ppHeaps[] = { cbuffer.cbvHeap.Get() };
		commandList_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		// Bind the current frame's constant buffer to the pipeline.
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(cbuffer.cbvHeap->GetGPUDescriptorHandleForHeapStart(), m_deviceResources->GetCurrentFrameIndex(), cbuffer.cbvDescriptorSize);
		commandList_->SetGraphicsRootDescriptorTable(0, gpuHandle);
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
