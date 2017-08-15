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
	void SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, uint8_t* uniforms, const Model& model);
private:
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	commandList_;
	struct Buffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_GPU_VIRTUAL_ADDRESS bufferLocation;
		size_t size, elementSize;
	};
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> bufferUploads_;
	std::vector<Buffer> buffers_;
	struct CBuffer {
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;
		UINT8* mappedConstantBuffer = nullptr;
		UINT cbvDescriptorSize, alignedConstantBufferSize;
		size_t dataSize;
		~CBuffer() {
			constantBuffer->Unmap(0, nullptr);
		}
	};
	std::vector<CBuffer> cbuffers_;
	CBuffer CreateAndMapConstantBuffers(size_t size);

	D3D12_RECT m_scissorRect;
	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	bool loadingComplete_ = false;
};
