#pragma once
#include <vector>
#include "compatibility.h"
#include "Img.h"
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
#include "..\Common\StepTimer.h"
#include "PipelineState.h"
#include "DescriptorHeapAllocator.h"
#include "CBFrameAlloc.h"
#include "DescriptorFrameAlloc.h"
#include "..\source\cpp\ShaderStructures.h"

struct Mesh;

struct Dim {
	uint32_t w, h;
};

class Renderer {
	PipelineStates pipelineStates_;
public:
	Renderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
	~Renderer();
	void CreateDeviceDependentResources();
	void CreateWindowSizeDependentResources();
	void Update(DX::StepTimer const& timer);
	uint32_t GetCurrenFrameIndex() const;
	bool Render();
	void SaveState();

	void BeginUploadResources();
	BufferIndex CreateBuffer(const void* buffer, size_t sizeInBytes);
	TextureIndex CreateTexture(const void* buffer, uint64_t width, uint32_t height, Img::PixelFormat format, const char* label);
	Dim GetDimensions(TextureIndex);
	void EndUploadResources();

	TextureIndex CreateTexture(const void* data, uint64_t width, uint32_t height, Img::PixelFormat format, const char* label = nullptr);
	TextureIndex GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint64_t dim, ShaderId shader, bool mip, const char* label = nullptr);
	TextureIndex GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint64_t dim, ShaderId shader, const char* label = nullptr);
	TextureIndex GenBRDFLUT(uint64_t dim, ShaderId shader, const char* label = nullptr);
	TextureIndex GenTestCubeMap();

	void BeginRender();
	void StartForwardPass();
	void StartGeometryPass();
	void Submit(const ShaderStructures::DrawCmd& cmd);
	void DoLightingPass(const ShaderStructures::DeferredCmd& cmd);
	//ResourceHeapHandle GetStaticShaderResourceHeap(unsigned short descCountNeeded);
	//ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, size_t size, unsigned short count);
	//void UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size);
	//ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, BufferIndex textureIndex);

	std::shared_ptr<DX::DeviceResources> m_deviceResources;
private:
	void DownsampleDepth(ID3D12GraphicsCommandList*);
	struct {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources;
	}bufferUpload_;

	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> commandAllocators_;
	std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> commandLists_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> deferredCommandList_;

	struct Buffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_GPU_VIRTUAL_ADDRESS bufferLocation;
		size_t size;
		DXGI_FORMAT format;
	};
	std::vector<Buffer> buffers_;

	DescriptorFrameAlloc rtvDesc_;
	DescriptorFrameAlloc::Entry rtvEntry_;
	// for per frame dynamic data
	struct {
		CBFrameAlloc cb;
		DescriptorFrameAlloc desc;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> deferredCommandAllocator;
	}frames_[ShaderStructures::FrameCount], *frame_;

	Microsoft::WRL::ComPtr<ID3D12Resource> rtt_[_countof(PipelineState::deferredRTFmts)];
	BufferIndex fsQuad_ = InvalidBuffer;

	D3D12_RECT scissorRect_;
	bool loadingComplete_ = false;
};
