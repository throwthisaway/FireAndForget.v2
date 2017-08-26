#pragma once
#include <wrl.h>
#include <ppltasks.h>

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
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvHeap;
		std::vector<CBuffer> cbuffers;
	};
	static const size_t ROOT_VS_1CB = 0;
	static const size_t ROOT_VS_1CB_PS_1CB = 1;
	std::vector<Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;
	std::vector<State> states_;
	Concurrency::task<void> completionTask_;
};

