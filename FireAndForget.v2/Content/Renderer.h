#pragma once
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"
#include "..\source\cpp\RendererWrapper.h"
#include "..\source\cpp\ShaderStructures.h"
#include "PipelineState.h"
#include "DescriptorHeapAllocator.h"
#include "CBFrameAlloc.h"
#include "DescriptorFrameAlloc.h"
#include "..\source\cpp\Assets.hpp"

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
	BufferIndex CreateBuffer(const void* buffer, size_t sizeInBytes);
	BufferIndex CreateTexture(const void* buffer, UINT64 width, UINT height, UINT bytesPerPixel, DXGI_FORMAT format);
	void EndUploadResources();

	// Shader resources
	ShaderResourceIndex CreateShaderResource(uint32_t size, uint16_t count);
	void UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size);

	DescAllocEntryIndex AllocDescriptors(uint16_t count);
	void CreateCBV(DescAllocEntryIndex index, uint16_t offset, ShaderResourceIndex resourceIndex);
	void CreateCBV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, ShaderResourceIndex resourceIndex);
	void CreateSRV(DescAllocEntryIndex index, uint16_t offset, BufferIndex textureBufferIndex);
	void CreateSRV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, BufferIndex textureBufferIndex);
	void CreateSRV(DescAllocEntryIndex index, uint16_t offset, ID3D12Resource* resource, DXGI_FORMAT format);
	void CreateSRV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, ID3D12Resource* resource, DXGI_FORMAT format);
	

	void BeginRender();
	size_t StartRenderPass();
	template<typename CmdT>
	void Submit(const CmdT&) {
		assert(false); // TODO:: implement Submit overload
	}
	void SetDeferredBuffers(const ShaderStructures::DeferredBuffers& deferredBuffers);

	//ResourceHeapHandle GetStaticShaderResourceHeap(unsigned short descCountNeeded);
	//ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, size_t size, unsigned short count);
	//void UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size);
	//ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, BufferIndex textureIndex);

	std::shared_ptr<DX::DeviceResources> m_deviceResources;
private:
	struct {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources;
	}bufferUpload_;

	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> commandAllocators_;
	std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> commandLists_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> deferredCommandAllocators_[ShaderStructures::FrameCount];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> deferredCommandList_;

	struct Buffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_GPU_VIRTUAL_ADDRESS bufferLocation;
		size_t size;
		DXGI_FORMAT format;
	};
	std::vector<Buffer> buffers_;

	CBAlloc cbAlloc_;
	DescriptorAlloc descAlloc_[ShaderStructures::FrameCount];
	struct {
		CBFrameAlloc cb;
		DescriptorFrameAlloc desc;
	}frame_[ShaderStructures::FrameCount];

	Microsoft::WRL::ComPtr<ID3D12Resource> rtt_[ShaderStructures::RenderTargetCount];
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
	UINT rtvDescriptorSize_ = 0;
	BufferIndex fsQuad_ = InvalidBuffer;
	ShaderStructures::DeferredBuffers deferredBuffers_;

	D3D12_RECT m_scissorRect;
	bool loadingComplete_ = false;
};
