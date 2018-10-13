#include "pch.h"
#include "DescriptorFrameAlloc.h"
#include <d3d12.h>
#include "D3DHelpers.h"
#include "..\source\cpp\BufferUtils.h"
#include "..\Common\DirectXHelper.h"
#include <string>

void DescriptorFrameAlloc::Request() {
	pool_.push_back(CreateDescriptorHeap(device_, type_, max_, shaderVisible_,
		(std::wstring(L"frame alloc. desc. heap #") + std::to_wstring(pool_.size())).c_str()));
	index_ = (UINT)pool_.size() - 1;
	offset_ = 0;
}
void DescriptorFrameAlloc::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT maxDesc, bool shaderVisible) {
	index_ = 0;
	offset_ = 0;
	device_ = device;
	max_ = maxDesc;
	type_ = type;
	shaderVisible_ = shaderVisible;
	descSize_ = device->GetDescriptorHandleIncrementSize(type);
	pool_.clear();
	Request();
}

DescriptorFrameAlloc::Entry DescriptorFrameAlloc::Push(UINT count) {
	assert(count <= max_);
	if (offset_ + count > max_ &&  ++index_>= pool_.size()) Request();
	auto heap = pool_[index_].Get();
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(heap->GetCPUDescriptorHandleForHeapStart(), offset_, descSize_);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(heap->GetGPUDescriptorHandleForHeapStart(), offset_, descSize_);
	offset_ += count;
	return { heap, cpuHandle, gpuHandle };
}

void DescriptorFrameAlloc::CreateCBV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, uint32_t size) {
	assert(size == (AlignTo<decltype(size), 256>(size)));
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = gpuAddress;
	desc.SizeInBytes = size;
	device_->CreateConstantBufferView(&desc, cpuHandle);
}

void DescriptorFrameAlloc::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* textureBuffer, DXGI_FORMAT format) {
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Texture2D.MipLevels = 1;
	device_->CreateShaderResourceView(textureBuffer, &desc, cpuHandle);
}
void DescriptorFrameAlloc::Reset() {
	index_ = 0;
	offset_ = 0;
}
