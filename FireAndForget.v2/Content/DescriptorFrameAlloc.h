#pragma once
#include <type_traits>
#include <vector>
#include <wrl.h>
#include "CBFrameAlloc.h"

struct ID3D12Device;
struct ID3D12Resource;
class DescriptorFrameAlloc {
	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> pool_;
	UINT index_ = 0, max_ = 0, offset_ = 0;
	ID3D12Device* device_ = nullptr;
	D3D12_DESCRIPTOR_HEAP_TYPE type_;
	bool shaderVisible_;
	UINT descSize_;
	void Request();
public:
	UINT GetDescriptorSize() const { return descSize_; }
	void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDesc, bool shaderVisible);
	struct Entry {
		ID3D12DescriptorHeap* heap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	};
	Entry Push(UINT count);
	void CreateCBV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, uint32_t size);
	void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource);
	void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, UINT arrayIndex);
	void CreateCubeSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource);
	void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, UINT mipSlice = 0);
	void CreateArrayUAV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, UINT arrayIndex, UINT mipSlice = 0);
	void CreateRTV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, DXGI_FORMAT format);
	void CreateDSV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, ID3D12Resource* resource, DXGI_FORMAT format);
	void BindCBV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, const CBFrameAlloc::Entry& cb);
	void BindCBV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, UINT size);
	void BindSRV(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, ID3D12Resource* resource);
	void Reset();

};