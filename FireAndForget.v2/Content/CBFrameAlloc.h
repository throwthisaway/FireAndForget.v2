#pragma once
#include <type_traits>
#include <vector>
#include <wrl.h>

struct ID3D12Device;
struct ID3D12Resource;
class CBFrameAlloc {
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> pool_;
	UINT index_;
	uint64_t max_, offset_;
	D3D12_GPU_VIRTUAL_ADDRESS GPUAddressBase_ = -1;
	uint8_t* mappedBufferBase_ = nullptr;
	ID3D12Device* device_;
	void Request();
	void Map(UINT index);
	void Unmap(UINT index);
public:
	void Fill(uint8_t val);
	void Init(ID3D12Device* device, uint64_t bufferSize);
	struct Entry {
		ID3D12Resource* resource;
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
		uint8_t* cpuAddress;
		UINT size;
	};
	Entry Alloc(unsigned int size);
	template<typename T> Entry Upload(const T& data) {
		auto cb = Alloc(sizeof(T));
		memcpy(cb.cpuAddress, &data, sizeof(T));
		return cb;
	}
	void Reset();
};