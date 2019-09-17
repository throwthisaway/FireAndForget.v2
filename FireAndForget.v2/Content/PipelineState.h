#pragma once
#include <wrl.h>
#include <ppltasks.h>
#include "..\source\cpp\ShaderStructures.h"

namespace DX {
	class DeviceResources;
}

class PipelineStates {
	ID3D12Device* device;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pre, rg16, depth, forward, geometry, lighting, ssao, r32;
	std::vector<Concurrency::task<void>> shaderTasks;
public:
	enum class RTTs{Albedo, Normal, Material, Debug};
	static constexpr DXGI_FORMAT deferredRTFmts[] = {
		DXGI_FORMAT_R8G8B8A8_UNORM, // albedo
		DXGI_FORMAT_R16G16B16A16_FLOAT,	// normals
		DXGI_FORMAT_R8G8B8A8_UNORM, // material properties
#ifdef DEBUG_RT
		DXGI_FORMAT_R32G32B32A32_FLOAT, // debug
#endif
	};
	PipelineStates(ID3D12Device* device, DXGI_FORMAT backbufferFormat, DXGI_FORMAT depthbufferFormat);
	~PipelineStates();
	void CreateDeviceDependentResources();
	struct State {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		enum class RenderPass {Pre, Forward, Geometry, AO, Lighting, Post} pass;
	};
	std::vector<State> states_;
	Concurrency::task<void> CreateShader(ShaderId id,
		size_t rootSignatureIndex,
		const wchar_t* vs, 
		const wchar_t* ps, 
		const wchar_t* gs,
		const D3D12_INPUT_LAYOUT_DESC il,
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
		State::RenderPass pass);
	Concurrency::task<void> CreateComputeShader(ShaderId id,
		size_t rootSignatureIndex,
		const wchar_t* cs, 
		State::RenderPass pass);
	static const int ROOT_UNKNOWN = -1;
	static const int ROOT_VS_1CB_PS_1CB = 0;
	static const int ROOT_VS_1CB_PS_1TX_1CB = 1;
	static const int ROOT_SIG_COUNT = 2;
	std::vector<Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;

	Concurrency::task<void> completionTask_;

	//template<typename... T> class BufferTraits {};
	//struct Pos {
	//	size_t pipelineIndex;
	//	using Buffers = BufferTraits<Material::cMVP, Material::cColor>;
	//}pos_;

};
