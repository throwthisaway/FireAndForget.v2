#include "pch.h"
#include "DescriptorHeapAllocator.h"
#include "..\Common\DirectXHelper.h"
#include "..\source\cpp\BufferUtils.h"

using namespace Microsoft::WRL;

namespace {
	auto CreateDescriptorHeapForCBV_SRV_UAV(ID3D12Device* d3dDevice, UINT numDesc) {
		ComPtr<ID3D12DescriptorHeap> cbvHeap;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = numDesc;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbvHeap)));

		NAME_D3D12_OBJECT(cbvHeap);
		return cbvHeap;
	}

	ComPtr<ID3D12Resource> CreateConstantBuffer(ID3D12Device* d3dDevice, size_t size) {
		ComPtr<ID3D12Resource> constantBuffer;
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&constantBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBuffer)));

		NAME_D3D12_OBJECT(constantBuffer);
		return constantBuffer;
	}

	//https://stackoverflow.com/questions/12024304/c11-number-of-variadic-template-function-parameters

	template<size_t ...>
	struct SizeOf : std::integral_constant<std::size_t, 0> {};

	template<size_t Size0, size_t... SizeX>
	struct SizeOf<Size0, SizeX...> : std::integral_constant<std::size_t, Size0 + SizeOf<SizeX...>::value > {};

	// Constant buffers must be 256-byte aligned.
	template<size_t ...>
	struct Aligned256SizeOf : std::integral_constant<std::size_t, 0> {};

	template<size_t Size0, size_t... SizeX>
	struct Aligned256SizeOf<Size0, SizeX...> : std::integral_constant<std::size_t, AlignTo<Size0, 256>(Size0) + Aligned256SizeOf<SizeX...>::value > {};

	//template<typename... BuffersT>
	//struct ShaderResourceDesc {};

	template <typename...>
	void SetupDescriptorHeapInternal(ID3D12Device* device, D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress, CD3DX12_CPU_DESCRIPTOR_HANDLE& cbvCpuHandle) {}

	template <typename BufferT, typename... BuffersT>
	void SetupDescriptorHeapInternal(ID3D12Device* device, D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress, CD3DX12_CPU_DESCRIPTOR_HANDLE& cbvCpuHandle) {
		UINT cbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		const UINT c_alignedConstantBufferSize = AlignTo256(sizeof(BufferT));
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = cbvGpuAddress;
		desc.SizeInBytes = c_alignedConstantBufferSize;
		device->CreateConstantBufferView(&desc, cbvCpuHandle);
		cbvGpuAddress += desc.SizeInBytes;
		cbvCpuHandle.Offset(cbvDescriptorSize);
		SetupDescriptorHeapInternal<BuffersT...>(device, cbvGpuAddress, cbvCpuHandle);
	}

	template <typename... BuffersT>
	void SetupDescriptorHeap(ID3D12Device* device, ID3D12DescriptorHeap* cbvHeap, ID3D12Resource* constantBuffer, unsigned short repeat) {
		D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = constantBuffer->GetGPUVirtualAddress();
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart());
		for (unsigned short n = 0; n < repeat; ++n) {
			SetupDescriptorHeapInternal(device, cbvGpuAddress, cbvCpuHandle);
		}
	}
}

void CBAlloc::Init(ID3D12Device* device, size_t bufferSize, size_t startCount, size_t maxCount) {
	device_ = device; maxSize_ = bufferSize; maxCount_ = maxCount;
	assert(startCount);
	assert(!maxCount || maxCount > startCount);
	assert(bufferSize);

	for (size_t i = 0; i < startCount; ++i)
		buffers_.push_back(CreateConstantBuffer(device, bufferSize));

	currentGPUAddressBase_ = buffers_.front()->GetGPUVirtualAddress();
	DX::ThrowIfFailed(buffers_.front()->Map(0, &CD3DX12_RANGE(0, 0) /* We do not intend to read from this resource on the CPU.*/, reinterpret_cast<void**>(&currentMappedBufferBase_)));
}
CBAlloc::~CBAlloc() {
	for (auto& buffer : buffers_) buffer->Unmap(0, nullptr);
}
ShaderResourceIndex CBAlloc::Push(uint32_t size, uint16_t count) {
	size = AlignTo<uint32_t,256>(size);
	if (maxSize_ - currentOffset_ < size * count ) {
		if (maxCount_ && maxCount_ <= currentBufferIndex_) return InvalidShaderResource;
		++currentBufferIndex_;  currentOffset_ = 0;
		if (buffers_.size() <= currentBufferIndex_) buffers_.push_back(CreateConstantBuffer(device_, maxSize_));

		currentGPUAddressBase_ = buffers_.back()->GetGPUVirtualAddress();
		DX::ThrowIfFailed(buffers_.back()->Map(0, &CD3DX12_RANGE(0, 0) /* We do not intend to read from this resource on the CPU.*/, reinterpret_cast<void**>(&currentMappedBufferBase_)));
	}
	auto startIndex = entries_.size();
	for (size_t i = 0; i < count; ++i)
		entries_.push_back({ size, currentMappedBufferBase_ + currentOffset_, currentGPUAddressBase_ + currentOffset_});
	currentOffset_ += size * count;
	return (ShaderResourceIndex)startIndex;
}

void DescriptorAlloc::Init(ID3D12Device* device, size_t descCount, size_t startHeapCount, size_t maxHeapCount) {
	device_ = device;
	maxDescCount_ = (InternalDescriptorIndex)descCount;
	maxHeapCount_ = (DescriptorHeapIndex)maxHeapCount;
	assert(startHeapCount);
	assert(!maxHeapCount || maxHeapCount > startHeapCount);
	assert(descCount);

	for (size_t i = 0; i < startHeapCount; ++i)
		descriptorHeaps_.push_back({ CreateDescriptorHeapForCBV_SRV_UAV(device, maxDescCount_) });
}
DescriptorAlloc::DescriptorHeapIndex DescriptorAlloc::EnsureAvailableHeap(InternalDescriptorIndex count) {
	for (DescriptorHeapIndex i = 0; i <=/*inclusive*/ currentHeapIndex_; ++i) {
		if (descriptorHeaps_[i].currentDescIndex + count < maxDescCount_) return i;
	}
	if (maxHeapCount_ && maxHeapCount_ <= currentHeapIndex_) return InvalidDescriptorHeap;
	++currentHeapIndex_;
	descriptorHeaps_.push_back({ CreateDescriptorHeapForCBV_SRV_UAV(device_, maxDescCount_) });
	return (DescriptorHeapIndex)descriptorHeaps_.size() - 1;
}

DescAllocEntryIndex DescriptorAlloc::Push(uint16_t count) {
	DescriptorHeapIndex heapIndex = EnsureAvailableHeap(count);
	if (heapIndex == InvalidDescriptorHeap) return InvalidDescAllocEntry;

	auto& heapEntry = descriptorHeaps_[heapIndex];
	entries_.push_back({ heapEntry.descriptorHeap.Get(), heapEntry.currentDescIndex, count });
	heapEntry.currentDescIndex += count;
	return (DescAllocEntryIndex)entries_.size() - 1;
}

const DescriptorAlloc::DescriptorEntry& DescriptorAlloc::Get(DescAllocEntryIndex index) const {
	assert(index < entries_.size());
	return entries_[index];
}
CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorAlloc::GetGPUHandle(DescAllocEntryIndex index, uint16_t offset) const {
	assert(index < entries_.size());
	assert(entries_[index].count > offset);
	CD3DX12_GPU_DESCRIPTOR_HANDLE handle(entries_[index].heap->GetGPUDescriptorHandleForHeapStart(), INT(entries_[index].descIndex + offset), device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	return handle;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorAlloc::GetCPUHandle(DescAllocEntryIndex index, uint16_t offset) const {
	assert(index < entries_.size());
	assert(entries_[index].count > offset);
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(entries_[index].heap->GetCPUDescriptorHandleForHeapStart(), INT(entries_[index].descIndex + offset), device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	return handle;
}

void DescriptorAlloc::CreateCBV(DescAllocEntryIndex index, uint16_t offset, D3D12_GPU_VIRTUAL_ADDRESS cbGPUAddress, uint32_t cbSize) {
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = cbGPUAddress;
	desc.SizeInBytes = cbSize;
	device_->CreateConstantBufferView(&desc, GetCPUHandle(index, offset));
}
void DescriptorAlloc::CreateSRV(DescAllocEntryIndex index, uint16_t offset, ID3D12Resource* textureBuffer, DXGI_FORMAT format) {
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Texture2D.MipLevels = 1;
	device_->CreateShaderResourceView(textureBuffer, &desc, GetCPUHandle(index, offset));
}
