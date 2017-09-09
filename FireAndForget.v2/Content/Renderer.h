#pragma once
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"
#include "..\source\cpp\RendererWrapper.h"
#include "PipelineState.h"

using namespace FireAndForget_v2;

struct Model;

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
	void EndUploadResources();

	void BeginRender();
	size_t StartRenderPass();
	void SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, const Materials::cBuffers& uniforms, const Model& model);
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
	
	std::vector<Buffer> buffers_;

	D3D12_RECT m_scissorRect;
	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	bool loadingComplete_ = false;
};
