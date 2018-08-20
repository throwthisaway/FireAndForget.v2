#pragma once
#include <type_traits>
#include <vector>
#include <wrl.h>

struct ID3D12Device;
struct ID3D12Resource;
class DescriptorFrameAlloc {
	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> pool_;
	int currentIndex_ = -1;
	UINT numDesc_ = 0, currentDesc_ = 0;
	ID3D12Device* device_ = nullptr;
	D3D12_DESCRIPTOR_HEAP_TYPE type_;
	bool shaderVisible_;
	UINT descSize_;
	void RequestHeap();
public:
	void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDesc, bool shaderVisible);
	struct Entry {
		ID3D12DescriptorHeap* heap;
		D3D12_GPU_DESCRIPTOR_HANDLE handle;
	};
	Entry CreateCBV(D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, uint32_t size);
	Entry CreateSRV(ID3D12Resource* textureBuffer, DXGI_FORMAT format);
	void Reset();
	void Clear();
};