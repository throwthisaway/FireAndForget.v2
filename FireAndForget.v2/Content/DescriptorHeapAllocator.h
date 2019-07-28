#pragma once
#include <wrl.h>
#include "../source/cpp/RendererTypes.h"

class CBAlloc {
	struct Entry {
		uint32_t alignedBufferSize;
		uint8_t* cpuAddress;
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
	};
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> buffers_;
	std::vector<Entry> entries_;

	size_t currentBufferIndex_ = 0;
	uint64_t currentOffset_ = 0;
	ID3D12Device* device_;
	size_t maxSize_, maxCount_;

	D3D12_GPU_VIRTUAL_ADDRESS currentGPUAddressBase_;
	uint8_t* currentMappedBufferBase_;
public:
	void Init(ID3D12Device* device, size_t bufferSize, size_t startCount = 1, size_t maxCount = 0);
	~CBAlloc();
	ShaderResourceIndex Push(uint32_t size, uint16_t count);
	inline D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress(ShaderResourceIndex index) const {
		assert(entries_.size() > index);
		return entries_[index].gpuAddress;
	}
	inline uint32_t GetSize(ShaderResourceIndex index) const {
		assert(entries_.size() > index);
		return entries_[index].alignedBufferSize;
	}
	inline uint8_t* GetCPUAddress(ShaderResourceIndex index) const {
		assert(entries_.size() > index);
		return entries_[index].cpuAddress;
	}
};

class DescriptorAlloc {
	struct HeapEntry {
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		uint16_t currentDescIndex = 0;
	};
	std::vector<HeapEntry> descriptorHeaps_;
public:
	struct DescriptorEntry;
private:
	std::vector<DescriptorEntry> entries_;

	ID3D12Device* device_;

	using InternalDescriptorIndex = uint16_t; 	// descriptor index in a heap
	InternalDescriptorIndex maxDescCount_;
	using DescriptorHeapIndex = uint16_t;	// index of HeapEntry
	DescriptorHeapIndex maxHeapCount_, currentHeapIndex_ = 0;
	const DescriptorHeapIndex InvalidDescriptorHeap = std::numeric_limits<DescriptorHeapIndex>::max();
	DescriptorHeapIndex EnsureAvailableHeap(InternalDescriptorIndex count);
public:
	struct DescriptorEntry {
		ID3D12DescriptorHeap* heap;
		uint16_t descIndex;
		uint16_t count;
	};
	using Index = uint32_t;
	static constexpr Index InvalidIndex = std::numeric_limits<Index>::max();
	void Init(ID3D12Device* device, size_t descCount, size_t startHeapCount = 1, size_t maxHeapCount = 0);
	DescAllocEntryIndex Push(uint16_t count);

	const DescriptorEntry& Get(DescAllocEntryIndex index) const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(DescAllocEntryIndex index, uint16_t offset) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(DescAllocEntryIndex index, uint16_t offset) const;


	void CreateCBV(DescAllocEntryIndex index, uint16_t offset, D3D12_GPU_VIRTUAL_ADDRESS cbGPUAddress, uint32_t cbSize);
	void CreateSRV(DescAllocEntryIndex index, uint16_t offset, ID3D12Resource* textureBuffer, DXGI_FORMAT format);
};
