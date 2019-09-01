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
//#define DXGI_ANALYSIS
#ifdef DXGI_ANALYSIS
#include <DXGItype.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <DXProgrammableCapture.h>
#endif
struct Mesh;

struct Dim {
	uint32_t w, h;
};

class Renderer {
public:
	Renderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, DXGI_FORMAT backBufferFormat);
	~Renderer();
	void CreateDeviceDependentResources();
	void CreateWindowSizeDependentResources();
	void Update(double frame, double total);
	uint32_t GetCurrenFrameIndex() const;
	bool Render();
	void SaveState();

	void BeginUploadResources();
	BufferIndex CreateBuffer(const void* buffer, size_t sizeInBytes);
	TextureIndex CreateTexture(const void* buffer, uint64_t width, uint32_t height, Img::PixelFormat format, LPCSTR label = nullptr);
	Dim GetDimensions(TextureIndex);
	void EndUploadResources();

	void BeginRender();
	void BeginPrePass();
	TextureIndex GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const ModoMeshLoader::Submesh& submesh, uint32_t dim, ShaderId shader, bool mip, LPCSTR label = nullptr);
	TextureIndex GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const ModoMeshLoader::Submesh& submesh, uint32_t dim, ShaderId shader, LPCSTR label = nullptr);
	TextureIndex GenBRDFLUT(uint32_t dim, ShaderId shader, LPCSTR label = nullptr);
	void EndPrePass();
	void StartForwardPass();
	void Submit(const ShaderStructures::BgCmd& cmd);
	void StartGeometryPass();
	void Submit(const ShaderStructures::DrawCmd& cmd);
	void Submit(const ShaderStructures::ModoDrawCmd&);
	void DoLightingPass(const ShaderStructures::DeferredCmd& cmd);

	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	bool Ready() const { return loadingComplete_; }
private:
	void DownsampleDepth(ID3D12GraphicsCommandList*);
	void GenMips(Microsoft::WRL::ComPtr<ID3D12Resource> resource, DXGI_FORMAT fmt, int width, int height, uint32_t arraySize);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTarget(DXGI_FORMAT format, UINT width, UINT height, D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clearValue = nullptr, LPCWSTR label = nullptr);

	DXGI_FORMAT backbufferFormat_, 
		depthresourceFormat_ = DXGI_FORMAT_R32_TYPELESS, 
		depthbufferFormat_ = DXGI_FORMAT_D32_FLOAT;
	PipelineStates pipelineStates_;

	struct {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources;
	}bufferUpload_;
	struct {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
		// TODO:: these are not used?
		//Microsoft::WRL::ComPtr<ID3D12CommandAllocator> computeCmdAllocator;
		//Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> computeCmdList;
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
	DescriptorFrameAlloc rtvDescAlloc_, dsvDescAlloc_;
	struct {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_RESOURCE_STATES state;
		DescriptorFrameAlloc::Entry view;
		void ResourceTransition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState) {
			if (state != afterState) {
				auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), state, afterState);
				state = afterState;
				commandList->ResourceBarrier(1, &barrier);
			}
		}
	}ao_, halfResDepth_, depthStencil_;
	struct {
		Microsoft::WRL::ComPtr<ID3D12Resource> res[_countof(PipelineStates::deferredRTFmts)];
		D3D12_RESOURCE_STATES state;
		DescriptorFrameAlloc::Entry view;
	}rtt_;
	struct {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource[ShaderStructures::FrameCount];
		D3D12_RESOURCE_STATES state[ShaderStructures::FrameCount];
		DescriptorFrameAlloc::Entry view;
		void ResourceTransition(ID3D12GraphicsCommandList* commandList, int index, D3D12_RESOURCE_STATES afterState) {
			if (state[index] != afterState) {
				auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource[index].Get(), state[index], afterState);
				state[index] = afterState;
				commandList->ResourceBarrier(1, &barrier);
			}
		}
	}renderTargets_;
	ID3D12Resource* GetRenderTarget() { return renderTargets_.resource[GetCurrenFrameIndex()].Get(); }
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() {
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(renderTargets_.view.cpuHandle, GetCurrenFrameIndex(), rtvDescAlloc_.GetDescriptorSize());
	}
	D3D12_VIEWPORT GetViewport() const {
		auto size = m_deviceResources->GetOutputSize();
		return { 0.0f, 0.0f, size.Width, size.Height, 0.0f, 1.0f };
	}
	// for per frame dynamic data
	struct {
		CBFrameAlloc cb;
		DescriptorFrameAlloc desc;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> deferredCommandAllocator;
	}frames_[ShaderStructures::FrameCount], *frame_;

	
	//BufferIndex fsQuad_ = InvalidBuffer;

	D3D12_RECT scissorRect_;
	bool loadingComplete_ = false;
#ifdef DXGI_ANALYSIS
	Microsoft::WRL::ComPtr<IDXGraphicsAnalysis> pGraphicsAnalysis;
	bool graphicsDebugging = false;
#endif
};
