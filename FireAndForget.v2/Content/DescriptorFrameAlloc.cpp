#include "pch.h"
#include "DescriptorFrameAlloc.h"
#include <d3d12.h>
#include "D3DHelpers.h"
#include "..\source\cpp\BufferUtils.h"
#include "..\Common\DirectXHelper.h"

void DescriptorFrameAlloc::RequestHeap() {
	assert(device_);
	ID3D12DescriptorHeap* heap;
	if (++currentIndex_ >= pool_.size()) {
		pool_.push_back(CreateDescriptorHeap(device_, type_, numDesc_, shaderVisible_, L"HeapFrameAlloc"));
		heap = pool_.back().Get();
	}
	else heap = pool_[currentIndex_].Get();
	currentDesc_ = 0;
}
void DescriptorFrameAlloc::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDesc, bool shaderVisible) {
	// TODO:: clear is dangerous after device loss
	Clear();
	device_ = device;
	numDesc_ = numDesc;
	type_ = type;
	shaderVisible_ = shaderVisible;
	descSize_ = device->GetDescriptorHandleIncrementSize(type);
}
DescriptorFrameAlloc::Entry DescriptorFrameAlloc::CreateCBV(D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, uint32_t size) {
	if (currentDesc_ + 1 > numDesc_) RequestHeap();
	auto heap = pool_[currentIndex_].Get();
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(heap->GetCPUDescriptorHandleForHeapStart(), currentDesc_, descSize_);
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = gpuAddress;
	desc.SizeInBytes = size;
	device_->CreateConstantBufferView(&desc, cpuHandle);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(heap->GetGPUDescriptorHandleForHeapStart(), currentDesc_, descSize_);
	++currentDesc_;
	return { heap, gpuHandle };
}

DescriptorFrameAlloc::Entry DescriptorFrameAlloc::CreateSRV(ID3D12Resource* textureBuffer, DXGI_FORMAT format) {
	if (currentDesc_ + 1 > numDesc_) RequestHeap();
	auto heap = pool_[currentIndex_].Get();
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Texture2D.MipLevels = 1;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(heap->GetCPUDescriptorHandleForHeapStart(), currentDesc_, descSize_);
	device_->CreateShaderResourceView(textureBuffer, &desc, cpuHandle);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(heap->GetGPUDescriptorHandleForHeapStart(), currentDesc_, descSize_);
	++currentDesc_;
	return { heap, gpuHandle };
}
void DescriptorFrameAlloc::Reset() {
	currentIndex_ = -1;
	currentDesc_ = 0;
}
void DescriptorFrameAlloc::Clear() {
	Reset();
	pool_.clear();
}
