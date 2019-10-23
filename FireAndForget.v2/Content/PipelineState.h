#pragma once
#include <wrl.h>
#include <ppltasks.h>
#include "..\source\cpp\ShaderStructures.h"

namespace DX {
	class DeviceResources;
}

class PipelineStates {
public:
	enum class RTTs{Albedo, NormalWS, NormalVS, Material, Debug};
	static constexpr DXGI_FORMAT deferredRTFmts[] = {
		DXGI_FORMAT_R8G8B8A8_UNORM, // albedo
		DXGI_FORMAT_R16G16B16A16_FLOAT,	// normalsWS
		DXGI_FORMAT_R16G16B16A16_FLOAT,	// normalsVS view space normals without normalmaps for SSAO
		DXGI_FORMAT_R8G8B8A8_UNORM, // material properties
		DXGI_FORMAT_R32G32B32A32_FLOAT, // world position
	};
	PipelineStates(ID3D12Device* device, DXGI_FORMAT backbufferFormat, DXGI_FORMAT depthbufferFormat);
	~PipelineStates();
	void CreateDeviceDependentResources();
	struct State {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		enum class RenderPass {Pre, Forward, Geometry, AO, Lighting, Post, Shadow} pass;
	};
	State states[ShaderStructures::Count];
	Concurrency::task<void> completionTask;
	//template<typename... T> class BufferTraits {};
	//struct Pos {
	//	size_t pipelineIndex;
	//	using Buffers = BufferTraits<Material::cMVP, Material::cColor>;
	//}pos_;
private:
	using CreateShaderTask = Concurrency::task<void>;
	ID3D12Device* device;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pre, rg16, downsampleDepth, forward, geometry, lighting, ssao, r32, shadow;
	std::vector<CreateShaderTask> shaderTasks;

	CreateShaderTask CreateShadowShader(ShaderId id,
		size_t rootSignatureIndex,
		const D3D12_INPUT_LAYOUT_DESC il,
		const wchar_t* vs,
		const wchar_t* gs,
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
		State::RenderPass pass);
	CreateShaderTask CreateShader(ShaderId id,
		size_t rootSignatureIndex,
		const D3D12_INPUT_LAYOUT_DESC il,
		const wchar_t* vs, 
		const wchar_t* ps, 
		const wchar_t* gs,
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
		State::RenderPass pass);
	CreateShaderTask CreateComputeShader(ShaderId id,
		size_t rootSignatureIndex,
		const wchar_t* cs, 
		State::RenderPass pass);
	static const int ROOT_UNKNOWN = -1;
	static const int ROOT_VS_1CB_PS_1CB = 0;
	static const int ROOT_VS_1CB_PS_1TX_1CB = 1;
	static const int ROOT_VS_1CB = 2;
	static const int ROOT_SIG_COUNT = 3;
	std::vector<Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;

};
