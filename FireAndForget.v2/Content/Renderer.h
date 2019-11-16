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
	Windows::Foundation::Size GetWindowSize();
	void BeforeResize();
	void CreateWindowSizeDependentResources();
	void Update(double frame, double total);
	uint32_t GetCurrenFrameIndex() const;
	bool Render();
	void SaveState();

	void BeginUploadResources();
	BufferIndex CreateBuffer(const void* buffer, size_t sizeInBytes);
	TextureIndex CreateTexture(const void* buffer, uint64_t width, uint32_t height, Img::PixelFormat format, LPCSTR label = nullptr);
	RTIndex CreateShadowRT(UINT width, UINT height);
	Dim GetDimensions(TextureIndex);
	void EndUploadResources();

	void BeginRender();
	void BeginPrePass();
	TextureIndex GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const ModoMeshLoader::Submesh& submesh, uint32_t dim, ShaderId shader, bool mip, LPCSTR label = nullptr);
	TextureIndex GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const ModoMeshLoader::Submesh& submesh, uint32_t dim, ShaderId shader, LPCSTR label = nullptr);
	TextureIndex GenBRDFLUT(uint32_t dim, ShaderId shader, LPCSTR label = nullptr);
	TextureIndex GenConeMap(TextureIndex id, LPCSTR label = nullptr);
	void EndPrePass();
	void StartForwardPass();
	void Submit(const ShaderStructures::BgCmd& cmd);
	void StartGeometryPass();
	void Submit(const ShaderStructures::DrawCmd& cmd);
	void Submit(const ShaderStructures::ModoDrawCmd&);
	void SSAOPass(const ShaderStructures::SSAOCmd& cmd);
	void StartShadowPass(RTIndex rtIndex);
	void ShadowPass(const ShaderStructures::ShadowCmd&);
	void EndShadowPass(RTIndex rtIndex);
	void DoLightingPass(const ShaderStructures::DeferredCmd& cmd);

	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	bool Ready() const { return loadingComplete_; }
private:
	void DownsampleDepth(ID3D12GraphicsCommandList*);
	void GenMips(Microsoft::WRL::ComPtr<ID3D12Resource> resource, DXGI_FORMAT fmt, int width, int height, uint32_t arraySize);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTarget(DXGI_FORMAT format, UINT width, UINT height, D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clearValue = nullptr, LPCWSTR label = nullptr);

	void SSAOBlurPass(ID3D12GraphicsCommandList*);

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

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[ShaderStructures::FrameCount];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
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
	template<int ResourceCount>
	struct RT {
		Microsoft::WRL::ComPtr<ID3D12Resource> resources[ResourceCount];
		D3D12_RESOURCE_STATES states[ResourceCount];
		DescriptorFrameAlloc::Entry view;
		UINT width, height;
		void ResourceTransition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState) {
			CD3DX12_RESOURCE_BARRIER barriers[_countof(resources)];
			int count = 0;
			for (int i = 0; i < _countof(resources); ++i) {
				if (states[i] == afterState) continue;
				barriers[count] = CD3DX12_RESOURCE_BARRIER::Transition(resources[i].Get(), states[i], afterState);
				states[count] = afterState;
				++count;
			}
			if (count) commandList->ResourceBarrier(count, barriers);
		}
		void ResourceTransition(ID3D12GraphicsCommandList* commandList, int index, D3D12_RESOURCE_STATES afterState) {
			if (states[index] == afterState) return;
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resources[index].Get(), states[index], afterState);
			states[index] = afterState;
			commandList->ResourceBarrier(1, &barrier);
		}
		D3D12_VIEWPORT GetViewport() const {
			return { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
		}
		D3D12_RECT GetScissorRect() const {
			return { 0, 0, (LONG)width, (LONG)height };
		}
	};
	RT<1> halfResDepthRT_, depthStencil_, ssaoBlurRT_;
	RT<2> ssaoRT_;
	RT<_countof(PipelineStates::deferredRTFmts)> gbuffersRT_;
	RT<ShaderStructures::FrameCount> backBufferRTs_;
	std::vector<RT<1>> renderTargets_;
	template<int RTCount> void Setup(ID3D12GraphicsCommandList* commandList, ShaderId shaderId, const RT<RTCount>& rt, PCWSTR eventName = nullptr);
	RT<1> CreateDepthRT(UINT width, UINT height);
	// helper for retrieving the currently active final rendertarget
	ID3D12Resource* GetBackBuffer() { return backBufferRTs_.resources[GetCurrenFrameIndex()].Get(); }
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetBackBufferView() {
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(backBufferRTs_.view.cpuHandle, GetCurrenFrameIndex(), rtvDescAlloc_.GetDescriptorSize());
	}

	// for per frame dynamic data
	struct {
		CBFrameAlloc cb;
		DescriptorFrameAlloc desc;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> deferredCommandAllocator;
		template<typename T>
		void BindCBV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, const T& data) {
			desc.BindCBV(cpuHandle, cb.Upload(data));
		}
		void BindCBV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, UINT size) {
			desc.BindCBV(cpuHandle, gpuAddress, size);
		}
		void BindSRV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, ID3D12Resource* resource) {
			desc.BindSRV(cpuHandle, resource);
		}
		void BindCubeSRV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, ID3D12Resource* resource) {
			desc.BindCubeSRV(cpuHandle, resource);
		}
	}frames_[ShaderStructures::FrameCount], *frame_;

	struct SSAO {
		static const int kKernelSize = 14;
		size_t size256;
		Microsoft::WRL::ComPtr<ID3D12Resource> kernelResource = nullptr;
		static std::array<glm::vec4, kKernelSize> GenKernel();
	}ssao_;
	bool loadingComplete_ = false;
#ifdef DXGI_ANALYSIS
	Microsoft::WRL::ComPtr<IDXGraphicsAnalysis> pGraphicsAnalysis;
	bool graphicsDebugging = false;
#endif
};
