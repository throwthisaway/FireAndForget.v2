#pragma once
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"
#include "..\source\cpp\RendererWrapper.h"
#include "PipelineState.h"
#include "DescriptorHeapAllocator.h"

using namespace FireAndForget_v2;

struct Mesh;

class Renderer {
	PipelineStates pipelineStates_;
public:
	Renderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
	~Renderer();
	void CreateDeviceDependentResources();
	void CreateWindowSizeDependentResources();
	void Update(DX::StepTimer const& timer);
	bool Render();
	void SaveState();

	void BeginUploadResources();
	BufferIndex CreateBuffer(const void* buffer, size_t sizeInBytes, size_t elementSize);
	BufferIndex CreateTexture(const void* buffer, UINT width, UINT height, UINT bytesPerPixel, DXGI_FORMAT format);
	void EndUploadResources();

	void BeginRender();
	size_t StartRenderPass();
	void SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, ResourceHeapHandle shaderResourceHeap, const std::vector<size_t>& bufferIndices, const Mesh& model);

	ResourceHeapHandle GetStaticShaderResourceHeap(unsigned short descCountNeeded);
	ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, size_t size, unsigned short count);
	void UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size);
	ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, BufferIndex textureIndex);

	std::shared_ptr<DX::DeviceResources> m_deviceResources;
private:
	struct {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources;
	}bufferUpload_;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> frameCommandList_;

	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> commandAllocators_;
	std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> commandLists_;

	struct Buffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_GPU_VIRTUAL_ADDRESS bufferLocation;
		size_t size, elementSize;
		DXGI_FORMAT format;
	};
	ShaderResources shaderResources_;
	std::vector<Buffer> buffers_;
	
	std::vector<StackAlloc::FrameDesc&> shaderResourceDescriptors_;
	D3D12_RECT m_scissorRect;
	bool loadingComplete_ = false;
};
