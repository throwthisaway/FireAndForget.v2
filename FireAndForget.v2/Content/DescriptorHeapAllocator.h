#pragma once
#include <wrl.h>
#include "../source/cpp/RendererTypes.h"

struct ShaderResource {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> cbuffer;
	size_t count, size;
};

class StackAlloc {
	ID3D12Device* device_;
	size_t mappedConstantBufferOffset_ = 0;
public:
	ShaderResource resource_;
	D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress_;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle_;
	UINT8* mappedConstantBuffer_ = nullptr;
	UINT cbvDescriptorSize_;
	struct FrameDesc;
private:
	std::vector<FrameDesc> frames_;
public:
	using Index = unsigned short;
	struct FrameDesc {
		size_t alignedConstantBufferSize;
		UINT8* destination;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	};
	StackAlloc(ID3D12Device* device, size_t max, size_t size);
	~StackAlloc();
	Index PushPerFrameCBV(UINT size, unsigned short count);
	Index PushSRV(ID3D12Resource* textureBuffer, DXGI_FORMAT format);
	const FrameDesc& Get(size_t index);
	void Pop(size_t count) { /* TODO:: cbvCpuHandle.Offset(count * -cbvDescriptorSize_);*/}
};
class ShaderResources {
	StackAlloc staticResources_;
public:
	ResourceHeapHandle GetCurrentShaderResourceHeap(unsigned short descCountNeeded);
	StackAlloc& GetShaderResourceHeap(ResourceHeapHandle);
	ShaderResources(ID3D12Device* device);
};
