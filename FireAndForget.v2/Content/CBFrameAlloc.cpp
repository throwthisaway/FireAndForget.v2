#include "pch.h"
#include "CBFrameAlloc.h"
#include <d3d12.h>
#include "D3DHelpers.h"
#include "..\source\cpp\BufferUtils.h"
#include "..\Common\DirectXHelper.h"
#include <string>
#undef max
void CBFrameAlloc::Unmap(UINT index) {
	assert(index < pool_.size());
	pool_[index]->Unmap(0, &CD3DX12_RANGE(0, 0));
}
void CBFrameAlloc::Map(UINT index) {
	assert(index < pool_.size());
	ID3D12Resource* resource = pool_[index].Get();
	GPUAddressBase_ = resource->GetGPUVirtualAddress();
	assert(GPUAddressBase_ != -1);
	DX::ThrowIfFailed(resource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&mappedBufferBase_)));
}
void CBFrameAlloc::Request() {
	pool_.push_back(CreateConstantBuffer(device_, max_, 
		(std::wstring(L"frame alloc. constant buffer #") + std::to_wstring(pool_.size())).c_str()));
	index_ = UINT(pool_.size() - 1);
	Map(index_);
}
// Call after device acquired
void CBFrameAlloc::Init(ID3D12Device* device, uint64_t bufferSize) {
	GPUAddressBase_ = -1;
	mappedBufferBase_ = nullptr;
	device_ = device;
	max_ = bufferSize;
	pool_.clear();
	Request();
}
CBFrameAlloc::Entry CBFrameAlloc::Alloc(unsigned int size) {
	size = AlignTo<256>(size);
	if (offset_ + size >= max_) {
		if (index_ < pool_.size()) Unmap(index_);
		++index_;
		if (index_ >= pool_.size()) Request();
		else Map(index_);
		offset_ = 0;
	}
	Entry result = { pool_[index_].Get(), GPUAddressBase_ + offset_, mappedBufferBase_ + offset_, size};
	offset_ += size;
	return result;
}
void CBFrameAlloc::Fill(uint8_t val) {
	memset(mappedBufferBase_, val, max_);
}
// Call beginning of a frame
void CBFrameAlloc::Reset() {
	if (index_ != 0) {
		Unmap(index_);
		Map(0);
	}
	index_ = 0;
	offset_ = 0;
	
}
