#include "pch.h"
#include "Renderer.h"
#include "..\Common\DirectXHelper.h"
#include "..\source\cpp\VertexTypeAliases.h"
#include "..\source\cpp\VertexTypes.h"
#include "..\source\cpp\DeferredBindings.h"
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

using namespace Microsoft::WRL;
using namespace DirectX;
using namespace Windows::Foundation;

namespace {
	static constexpr size_t defaultDescCount = 256, defaultBufferSize = 65536, defaultCBFrameAllocSize = 65536, defaultDescFrameAllocCount = 256;
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
uint32_t Renderer::GetCurrenFrameIndex() const {
	return m_deviceResources->GetCurrentFrameIndex();
}
Dim Renderer::GetDimensions(TextureIndex index) {
	D3D12_RESOURCE_DESC desc = buffers_[index].resource->GetDesc();
	return { (decltype(Dim::w))desc.Width, (decltype(Dim::h))desc.Height };
}
TextureIndex Renderer::CreateTexture(const void* buffer, uint64_t width, uint32_t height, Img::PixelFormat format, const char* label) {
	DXGI_FORMAT dxgiFmt = PixelFormatToDXGIFormat(format);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(dxgiFmt, width, height);

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
	auto device = m_deviceResources->GetD3DDevice();
	rtvDesc_.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, _countof(PipelineStates::deferredRTFmts), true);
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

	for (size_t i = 0; i < pipelineStates_.states_.size(); ++i) {
		if (pipelineStates_.states_[i].pass != PipelineStates::State::RenderPass::Lighting) continue;
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
		NAME_D3D12_OBJECT(commandList);
		commandLists_.push_back(commandList);
	}

	BeginUploadResources();
	// fullscreen quad...
	VertexFSQuad quad[] = { { { -1., -1.f },{ 0.f, 1.f } },{ { -1., 1.f },{ 0.f, 0.f } },{ { 1., -1.f },{ 1.f, 1.f } },{ { 1., 1.f },{ 1.f, 0.f } } };
	fsQuad_ = CreateBuffer(quad, sizeof(quad));
	EndUploadResources();
}

void Renderer::CreateWindowSizeDependentResources() {
	Size outputSize = m_deviceResources->GetOutputSize();
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	scissorRect_ = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };

	rtvEntry_ = rtvDesc_.Push(_countof(PipelineStates::deferredRTFmts));
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor = rtvEntry_.cpuHandle;
	D3D12_CLEAR_VALUE clearValue;
	memcpy(clearValue.Color, Black, sizeof(clearValue.Color));
	for (int j = 0; j < _countof(PipelineStates::deferredRTFmts); ++j) {
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(PipelineStates::deferredRTFmts[j], (UINT)viewport.Width, (UINT)viewport.Height);
		clearValue.Format = PipelineStates::deferredRTFmts[j];
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
		desc.Format = PipelineStates::deferredRTFmts[j];
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		device->CreateRenderTargetView(rtt_[j].Get(), &desc, rtvDescriptor);
		rtvDescriptor.Offset(rtvDesc_.GetDescriptorSize());
	}
}

void Renderer::Update(DX::StepTimer const& timer) {
	if (loadingComplete_){
	}
}

void Renderer::BeginRender() {
	if (!loadingComplete_) return;

	frame_ = &frames_[m_deviceResources->GetCurrentFrameIndex()];
	frame_->cb.Reset();
	frame_->desc.Reset();
	m_deviceResources->GetCommandAllocator()->Reset();
	size_t i = 0;
	for (auto& commandList : commandLists_) {
		auto* commandAllocator = commandAllocators_[m_deviceResources->GetCurrentFrameIndex() + i * FrameCount].Get();
		DX::ThrowIfFailed(commandAllocator->Reset());
		DX::ThrowIfFailed(commandList->Reset(commandAllocator, nullptr));
		++i;
	}

	// Initialise depth
	auto* commandList = commandLists_[0].Get();
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect_);
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetDepthStencil(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	commandList->ResourceBarrier(1, &barrier);
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
	// Indicate this resource will be in use as a render target.
	// TODO:: done in StartForwardPass renderTargetResourceBarriers[RenderTargetCount] = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetDepthStencil(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	//D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_deviceResources->GetRenderTargetView();

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
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(rtvEntry_.cpuHandle);
			// TODO:: should not clean downsampled depth RT
			for (int j = 0; j < _countof(PipelineStates::deferredRTFmts); ++j) {
				commandList->ClearRenderTargetView(handle, Black, 0, nullptr);
				handle.Offset(rtvDesc_.GetDescriptorSize());
			}
		}
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_deviceResources->GetDepthStencilView();
		commandList->OMSetRenderTargets(_countof(PipelineStates::deferredRTFmts), &rtvEntry_.cpuHandle, true, &depthStencilView);
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

void Renderer::Submit(const DrawCmd& cmd) {
	if (!loadingComplete_) return;
	auto id = cmd.shader;
	auto& state = pipelineStates_.states_[id];
	ID3D12GraphicsCommandList* commandList = commandLists_[id].Get();
	PIXBeginEvent(commandList, 0, L"Submit"); {
		DescriptorFrameAlloc::Entry entry;
		UINT index = 0;
		const int descSize = frame_->desc.GetDescriptorSize();
		switch (id) {
		case ShaderStructures::Debug: {
			entry = frame_->desc.Push(2);
			{
				auto cb = frame_->cb.Alloc(sizeof(float4x4));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				memcpy(cb.cpuAddress, &cmd.mvp, sizeof(float4x4));
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize);
			{
				auto cb = frame_->cb.Alloc(sizeof(float4));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				memcpy(cb.cpuAddress, &cmd.material.albedo, sizeof(float4));
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize);
			break;
		}
		case ShaderStructures::Pos: {
			entry = frame_->desc.Push(2);
			{
				auto cb = frame_->cb.Alloc(sizeof(ShaderStructures::Object));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((ShaderStructures::Object*)cb.cpuAddress)->m = cmd.m;
				((ShaderStructures::Object*)cb.cpuAddress)->mvp = cmd.mvp;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize);
			{
				auto cb = frame_->cb.Alloc(sizeof(ShaderStructures::GPUMaterial));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((ShaderStructures::GPUMaterial*)cb.cpuAddress)->diffuse = cmd.material.albedo;
				((ShaderStructures::GPUMaterial*)cb.cpuAddress)->specular_power.x = cmd.material.metallic;
				((ShaderStructures::GPUMaterial*)cb.cpuAddress)->specular_power.y = cmd.material.roughness;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize);
			break;
		}
		case ShaderStructures::Tex: {
			entry = frame_->desc.Push(3);
			{
				auto cb = frame_->cb.Alloc(sizeof(ShaderStructures::Object));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((ShaderStructures::Object*)cb.cpuAddress)->m = cmd.m;
				((ShaderStructures::Object*)cb.cpuAddress)->mvp = cmd.mvp;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize);
			{
				auto& texture = buffers_[cmd.material.texAlbedo];
				frame_->desc.CreateSRV(entry.cpuHandle, texture.resource.Get(), texture.format);
			}
			entry.cpuHandle.Offset(descSize);
			{
				auto cb = frame_->cb.Alloc(sizeof(ShaderStructures::GPUMaterial));
				frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
				((ShaderStructures::GPUMaterial*)cb.cpuAddress)->diffuse = cmd.material.albedo;
				((ShaderStructures::GPUMaterial*)cb.cpuAddress)->specular_power.x = cmd.material.metallic;
				((ShaderStructures::GPUMaterial*)cb.cpuAddress)->specular_power.y = cmd.material.roughness;
			}
			commandList->SetGraphicsRootDescriptorTable(index, entry.gpuHandle);
			++index; entry.cpuHandle.Offset(descSize);
			break;
		}
		default: assert(false);
		}

		ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
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
void Renderer::DownsampleDepth(ID3D12GraphicsCommandList* commandList) {
	PIXBeginEvent(commandList, 0, L"DownsampleDepth");
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetDepthStencil(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	const float factor = .5f;
	const uint32_t step = 2;
	viewport.Width *= factor; viewport.Height *= factor;
	commandList->RSSetViewports(1, &viewport);
	D3D12_RECT scissorRect = scissorRect_;
	scissorRect.left *= factor; scissorRect.bottom *= factor;
	commandList->RSSetScissorRects(1, &scissorRect);
	commandList->ResourceBarrier(1, &barrier);
	auto& state = pipelineStates_.states_[Downsample];
	commandList->SetGraphicsRootSignature(state.rootSignature.Get());
	commandList->SetPipelineState(state.pipelineState.Get());
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv;
	rtv.InitOffsetted(rtvEntry_.cpuHandle, rtvDesc_.GetDescriptorSize() * BINDING_DOWNSAMPLE_DEPTH);
	commandList->OMSetRenderTargets(1, &rtv, false, nullptr);

	auto cb = frame_->cb.Alloc(sizeof(uint32_t));
	*((uint32_t*)cb.cpuAddress) = step;
	auto entry = frame_->desc.Push(2);
	frame_->desc.CreateCBV(entry.cpuHandle, cb.gpuAddress, cb.size);
	const int descSize = frame_->desc.GetDescriptorSize();
	entry.cpuHandle.Offset(descSize);
	frame_->desc.CreateSRV(entry.cpuHandle, rtt_[BINDING_DOWNSAMPLE_DEPTH], PipelineStates::deferredRTFmts[BINDING_DOWNSAMPLE_DEPTH]);
	ID3D12DescriptorHeap* ppHeaps[] = { entry.heap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	commandList->SetGraphicsRootDescriptorTable(0, entry.gpuHandle);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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
	DX::ThrowIfFailed(frame_->deferredCommandAllocator->Reset());
	DX::ThrowIfFailed(commandList->Reset(frame_->deferredCommandAllocator.Get(), nullptr));
	DownsampleDepth(commandList);

	PIXBeginEvent(commandList, 0, L"DoLightingPass");
	{
		CD3DX12_RESOURCE_BARRIER presentResourceBarriers[RenderTargetCount];
		for (int i = 0; i < RenderTargetCount; ++i)
			presentResourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(rtt_[i].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
			frame_->desc.CreateSRV(entry.cpuHandle, rtt_[j].Get(), PipelineStates::deferredRTFmts[j]);
			entry.cpuHandle.Offset(descSize);
		}
		// Depth
		frame_->desc.CreateSRV(entry.cpuHandle, m_deviceResources->GetDepthStencil(), DXGI_FORMAT_R32_FLOAT);
		entry.cpuHandle.Offset(descSize);

		// Irradiance
		frame_->desc.CreateSRV(entry.cpuHandle, buffers_[cmd.irradiance].resource.Get(), buffers_[cmd.irradiance].format);
		entry.cpuHandle.Offset(descSize);
		// Prefiltered env. map
		frame_->desc.CreateSRV(entry.cpuHandle, buffers_[cmd.prefilteredEnvMap].resource.Get(), buffers_[cmd.prefilteredEnvMap].format);
		entry.cpuHandle.Offset(descSize);
		// BRDFLUT
		frame_->desc.CreateSRV(entry.cpuHandle, buffers_[cmd.BRDFLUT].resource.Get(), buffers_[cmd.BRDFLUT].format);
		entry.cpuHandle.Offset(descSize);
		// Random
		frame_->desc.CreateSRV(entry.cpuHandle, buffers_[cmd.random].resource.Get(), buffers_[cmd.random].format);
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
		CD3DX12_RESOURCE_BARRIER presentResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList->ResourceBarrier(1, &presentResourceBarrier);
	}
	PIXEndEvent(commandList);
}
