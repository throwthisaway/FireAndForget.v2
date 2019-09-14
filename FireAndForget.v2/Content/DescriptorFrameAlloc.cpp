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
	if (offset_ + count >= max_) {
		if (++index_ >= pool_.size()) Request();
		offset_ = 0;
	}
	auto heap = pool_[index_].Get();
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(heap->GetCPUDescriptorHandleForHeapStart(), offset_, descSize_);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(heap->GetGPUDescriptorHandleForHeapStart(), offset_, descSize_);
	offset_ += count;
	return { heap, cpuHandle, gpuHandle };
}

void DescriptorFrameAlloc::CreateCBV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, uint32_t size) {
	assert(size == (AlignTo<256>(size)));
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = gpuAddress;
	desc.SizeInBytes = size;
	device_->CreateConstantBufferView(&desc, cpuHandle);
}

void DescriptorFrameAlloc::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource) {
	const auto desc = resource->GetDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Format = (desc.Format == DXGI_FORMAT_R32_TYPELESS) ? DXGI_FORMAT_R32_FLOAT : desc.Format;
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.Texture2D.MipLevels = (!desc.MipLevels) ? UINT(-1) : desc.MipLevels;
	device_->CreateShaderResourceView(resource, &srv, cpuHandle);
}
void DescriptorFrameAlloc::CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, UINT arrayIndex) {
	const auto desc = resource->GetDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Format = desc.Format;
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.Texture2DArray.MipLevels = (!desc.MipLevels) ? UINT(-1) : desc.MipLevels;
	srv.Texture2DArray.ArraySize = 1;
	srv.Texture2DArray.FirstArraySlice = arrayIndex;
	device_->CreateShaderResourceView(resource, &srv, cpuHandle);
}
void DescriptorFrameAlloc::CreateCubeSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource) {
	const auto desc = resource->GetDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Format = desc.Format;
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.TextureCube.MipLevels = (!desc.MipLevels) ? UINT(-1) : desc.MipLevels;
	device_->CreateShaderResourceView(resource, &srv, cpuHandle);
}
void DescriptorFrameAlloc::CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, UINT mipSlice) {
	D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
	uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uav.Format = resource ? resource->GetDesc().Format : DXGI_FORMAT_R8G8B8A8_UNORM;
    uav.Texture2D.MipSlice = mipSlice;
	device_->CreateUnorderedAccessView(resource, nullptr, &uav, cpuHandle);
}
void DescriptorFrameAlloc::CreateArrayUAV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, UINT arrayIndex, UINT mipSlice) {
	D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
	uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uav.Format = resource ? resource->GetDesc().Format : DXGI_FORMAT_R8G8B8A8_UNORM;
    uav.Texture2DArray.MipSlice = mipSlice;
	uav.Texture2DArray.FirstArraySlice = resource ? arrayIndex : 0;
	uav.Texture2DArray.ArraySize = 1;//resource ? resource->GetDesc().DepthOrArraySize : 0;
	device_->CreateUnorderedAccessView(resource, nullptr, &uav, cpuHandle);
}
void DescriptorFrameAlloc::CreateRTV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, DXGI_FORMAT format) {
	D3D12_RENDER_TARGET_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	device_->CreateRenderTargetView(resource, &desc, cpuHandle);
}
void DescriptorFrameAlloc::CreateDSV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, DXGI_FORMAT format) {
	D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	desc.Flags = D3D12_DSV_FLAG_NONE;
	device_->CreateDepthStencilView(resource, &desc, cpuHandle);
}
void DescriptorFrameAlloc::BindCBV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, const CBFrameAlloc::Entry& cb) {
	BindCBV(cpuHandle, cb.gpuAddress, cb.size);
}
void DescriptorFrameAlloc::BindCBV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, UINT size) {
	CreateCBV(cpuHandle, gpuAddress, size);
	cpuHandle.Offset(descSize_);
}
void DescriptorFrameAlloc::BindSRV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, ID3D12Resource* resource) {
	CreateSRV(cpuHandle, resource);
	cpuHandle.Offset(descSize_);
}
void DescriptorFrameAlloc::Reset() {
	index_ = 0;
	offset_ = 0;
}
