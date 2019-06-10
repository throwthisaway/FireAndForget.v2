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
	TextureIndex CreateTexture(const void* buffer, uint64_t width, uint32_t height, Img::PixelFormat format, LPCWSTR label = nullptr);
	Dim GetDimensions(TextureIndex);
	void EndUploadResources();

	void BeginRender();
	void BeginPrePass();
	TextureIndex GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint32_t dim, ShaderId shader, bool mip, LPCWSTR label = nullptr);
	TextureIndex GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint32_t dim, ShaderId shader, LPCWSTR label = nullptr);
	TextureIndex GenBRDFLUT(uint32_t dim, ShaderId shader, LPCWSTR label = nullptr);
	void EndPrePass();
	void StartForwardPass();
	void Submit(const ShaderStructures::BgCmd& cmd);
	void StartGeometryPass();
	void Submit(const ShaderStructures::DrawCmd& cmd);
	void DoLightingPass(const ShaderStructures::DeferredCmd& cmd);

	std::shared_ptr<DX::DeviceResources> m_deviceResources;
private:
	void DownsampleDepth(ID3D12GraphicsCommandList*);
	void GenMips(Microsoft::WRL::ComPtr<ID3D12Resource> resource, DXGI_FORMAT fmt, int width, int height, uint32_t arraySize);
	struct {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources;
	}bufferUpload_;
	struct {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, computeCmdList;
		struct {
			DescriptorFrameAlloc desc;
		}rtv;
		CBFrameAlloc cb;
		DescriptorFrameAlloc desc;
	}prePass_;

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

	// render targets
	struct {
		DescriptorFrameAlloc desc;
		DescriptorFrameAlloc::Entry entry;
	}rtv_;
	Microsoft::WRL::ComPtr<ID3D12Resource> rtt_[_countof(PipelineStates::deferredRTFmts)];

	// for per frame dynamic data
	struct {
		CBFrameAlloc cb;
		DescriptorFrameAlloc desc;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> deferredCommandAllocator;
	}frames_[ShaderStructures::FrameCount], *frame_;

	
	BufferIndex fsQuad_ = InvalidBuffer;

	D3D12_RECT scissorRect_;
	bool loadingComplete_ = false;
};
