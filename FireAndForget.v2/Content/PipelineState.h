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

	struct State {
		size_t rootSignatureId;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	};
	std::vector<Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;
	std::vector<State> states_;
	Concurrency::task<void> completionTask_;
};

