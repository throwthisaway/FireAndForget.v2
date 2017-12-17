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
	size_t CreateBuffer(const void* buffer, size_t length, size_t elementSize);
	size_t CreateTexture(const void* buffer, UINT width, UINT height, UINT bytesPerPixel, DXGI_FORMAT format);
	void EndUploadResources();

	void BeginRender();
	size_t StartRenderPass();
	void SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, const std::vector<size_t>& bufferIndices, const Mesh& model);
	ShaderResources shaderResources_;
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
	};
	
	struct Texture {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		DXGI_FORMAT format;
	};
	std::vector<Buffer> buffers_;
	std::vector<Texture> textures_;
	D3D12_RECT m_scissorRect;
	bool loadingComplete_ = false;
};
