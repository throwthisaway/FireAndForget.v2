#pragma once
#include <type_traits>
#include <vector>
#include <wrl.h>

struct ID3D12Device;
struct ID3D12Resource;
class CBFrameAlloc {
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> pool_;
	int currentIndex_ = -1;
	uint64_t bufferSize_ = 0, currentSize_ = 0;
	D3D12_GPU_VIRTUAL_ADDRESS currentGPUAddressBase_ = -1;
	uint8_t* currentMappedBufferBase_ = nullptr;
	ID3D12Device* device_ = nullptr;
	void CreateNewBuffer();
public:
	void Init(ID3D12Device* device, uint64_t bufferSize);
	struct Entry {
		ID3D12Resource* resource;
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
		uint8_t* cpuAddress;
	};
	Entry Alloc(unsigned int size);
	void Reset();
	void Clear();
};