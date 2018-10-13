#pragma once
#include <wrl.h>
#include <ppltasks.h>
#include "..\source\cpp\ShaderStructures.h"

namespace DX {
	class DeviceResources;
}

class PipelineStates {
	const DX::DeviceResources* deviceResources_;
	bool loadingComplete_ = false;
public:
	static constexpr DXGI_FORMAT renderTargetFormats[] = {
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R16G16_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32G32B32A32_FLOAT
	};
	PipelineStates(const DX::DeviceResources*);
	~PipelineStates();
	void CreateDeviceDependentResources();
	Concurrency::task<void> CreateShader(ShaderId id, size_t rootSignatureIndex, const wchar_t* vs, const wchar_t* ps, const D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT count);
	Concurrency::task<void> CreateDeferredShader(ShaderId id, size_t rootSignatureIndex, const wchar_t* vs, const wchar_t* ps, const D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT count);
	struct CBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;
		UINT cbvDescriptorSize, alignedConstantBufferSize;
		size_t dataSize;
		UINT8* Map();
		void Unmap();
		~CBuffer();
	};

	static const size_t ROOT_VS_1CB_PS_1CB = 0;
	static const size_t ROOT_VS_1CB_PS_1TX_1CB = 1;
	static const size_t ROOT_VS_0CB_PS_1CB_5TX = 2;
	static const size_t ROOT_SIG_COUNT = 3;
	struct RootSig {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		std::vector<UINT> ranges;
	};
	std::vector<RootSig> rootSignatures_;
	struct State {
		size_t rootSignatureId;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		bool deferred;
		//std::function<ShaderResource(ID3D12Device*, unsigned short)> createShaderResource;
	};
	std::vector<State> states_;

	std::vector<Concurrency::task<void>> shaderTasks_;
	Concurrency::task<void> completionTask_;

	//template<typename... T> class BufferTraits {};
	//struct Pos {
	//	size_t pipelineIndex;
	//	using Buffers = BufferTraits<Material::cMVP, Material::cColor>;
	//}pos_;

};
