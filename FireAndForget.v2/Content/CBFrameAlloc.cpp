#include "pch.h"
#include "CBFrameAlloc.h"
#include <d3d12.h>
#include "D3DHelpers.h"
#include "..\source\cpp\BufferUtils.h"
#include "..\Common\DirectXHelper.h"

void CBFrameAlloc::CreateNewBuffer() {
	assert(device_);
	ID3D12Resource* resource;
	if (currentIndex_>= 0)
		pool_[currentIndex_]->Unmap(0, &CD3DX12_RANGE(0, 0));

	if (++currentIndex_ >= pool_.size()) {
		pool_.push_back(CreateConstantBuffer(device_, bufferSize_, L"CBFrameAlloc"));
		resource = pool_.back().Get();
	} else resource = pool_[currentIndex_].Get();
	
	currentGPUAddressBase_ = resource->GetGPUVirtualAddress();
	DX::ThrowIfFailed(resource->Map(0, &CD3DX12_RANGE(0, 0) , reinterpret_cast<void**>(&currentMappedBufferBase_)));
	currentSize_ = 0;
}	
void CBFrameAlloc::Init(ID3D12Device* device, uint64_t bufferSize) {
	// TODO:: clear is dangerous after device loss
	Clear();
	device_ = device;
	bufferSize_ = bufferSize;
}
CBFrameAlloc::Entry CBFrameAlloc::Alloc(unsigned int size) {
	size = AlignTo<decltype(size), 256>(size);
	currentSize_ += size;
	if (currentSize_ > bufferSize_) CreateNewBuffer();
	Entry result = { pool_[currentIndex_].Get(), currentGPUAddressBase_, currentMappedBufferBase_ };
	currentGPUAddressBase_ += size;
	currentMappedBufferBase_ += size;
	return result;
}
void CBFrameAlloc::Reset() {
	if (currentIndex_ >= 0)
		pool_[currentIndex_]->Unmap(0, &CD3DX12_RANGE(0, 0));
	currentIndex_ = -1;
	currentSize_ = 0;
}
void CBFrameAlloc::Clear() {
	Reset();
	pool_.clear();
}
