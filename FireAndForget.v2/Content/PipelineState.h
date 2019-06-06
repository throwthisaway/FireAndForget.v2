#pragma once
#include <wrl.h>
#include <ppltasks.h>
#include "..\source\cpp\ShaderStructures.h"

namespace DX {
	class DeviceResources;
}

class PipelineStates {
	const DX::DeviceResources* deviceResources_;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pre, rg16, depth, forward, deferred, post;
	std::vector<Concurrency::task<void>> shaderTasks;
public:
	static constexpr DXGI_FORMAT deferredRTFmts[] = {
		DXGI_FORMAT_R8G8B8A8_UNORM, // albedo
		DXGI_FORMAT_R16G16_UNORM,	// compressed normals
		DXGI_FORMAT_R8G8B8A8_UNORM, // material properties
		DXGI_FORMAT_R32G32B32A32_FLOAT, // debug
		DXGI_FORMAT_D32_FLOAT	// half-resolution depth
	};
	PipelineStates(const DX::DeviceResources*);
	~PipelineStates();
	void CreateDeviceDependentResources();
	Concurrency::task<void> CreateShader(ShaderId id,
		size_t rootSignatureIndex,
		const wchar_t* vs, 
		const wchar_t* ps, 
		const D3D12_INPUT_LAYOUT_DESC il,
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

	static const int ROOT_UNKNOWN = -1;
	static const int ROOT_VS_1CB_PS_1CB = 0;
	static const int ROOT_VS_1CB_PS_1TX_1CB = 1;
	static const int ROOT_SIG_COUNT = 2;
	std::vector<Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;
	struct State {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		enum class RenderPass {Pre, Forward, Geometry, Lighting, Post} pass;
		//std::function<ShaderResource(ID3D12Device*, unsigned short)> createShaderResource;
	};
	std::vector<State> states_;

	Concurrency::task<void> completionTask_;

	//template<typename... T> class BufferTraits {};
	//struct Pos {
	//	size_t pipelineIndex;
	//	using Buffers = BufferTraits<Material::cMVP, Material::cColor>;
	//}pos_;

};
