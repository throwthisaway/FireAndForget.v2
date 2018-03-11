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
	PipelineStates(const DX::DeviceResources*);
	~PipelineStates();
	void CreateDeviceDependentResources();
	Concurrency::task<void> CreateShader(ShaderStructures::ShaderId id, size_t rootSignatureIndex, const wchar_t* vs, const wchar_t* ps, const D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT count);
	struct CBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;
		UINT cbvDescriptorSize, alignedConstantBufferSize;
		size_t dataSize;
		UINT8* Map();
		void Unmap();
		~CBuffer();
	};

	struct State {
		size_t rootSignatureId;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		//std::function<ShaderResource(ID3D12Device*, unsigned short)> createShaderResource;
	};
	static const size_t ROOT_VS_1CB_PS_1CB = 0;
	static const size_t ROOT_VS_1CB_PS_1TX_2CB = 1;
	static const size_t ROOT_SIG_COUNT = 2;
	std::vector<Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;
	std::vector<State> states_;
	std::vector<Concurrency::task<void>> shaderTasks_;
	Concurrency::task<void> completionTask_;

	//template<typename... T> class BufferTraits {};
	//struct Pos {
	//	size_t pipelineIndex;
	//	using Buffers = BufferTraits<Material::cMVP, Material::cColor>;
	//}pos_;

};
