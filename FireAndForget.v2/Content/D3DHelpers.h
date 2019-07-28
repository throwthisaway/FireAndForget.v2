#pragma once
#include <wrl.h>
struct ID3D12Resource;
Microsoft::WRL::ComPtr<ID3D12Resource> CreateConstantBuffer(ID3D12Device* d3dDevice, size_t size, const wchar_t* name = nullptr);
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDesc, bool shaderVisible, const wchar_t* name = nullptr);