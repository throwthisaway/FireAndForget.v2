#include "pch.h"
#include "D3DHelpers.h"
#include <d3d12.h>
#include "..\Common\d3dx12.h"
#include "..\Common\DirectXHelper.h"

using namespace Microsoft::WRL;

ComPtr<ID3D12Resource> CreateConstantBuffer(ID3D12Device* d3dDevice, size_t size, const wchar_t* name) {
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

	if (name) DX::SetName(constantBuffer.Get(), name);
	return constantBuffer;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDesc, bool shaderVisible, const wchar_t* name) {
	ComPtr<ID3D12DescriptorHeap> heap;
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = numDesc;
	heapDesc.Type = type;
	// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
	heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)));
	if (name) DX::SetName(heap.Get(), name);
	return heap;
}
