#pragma once
#include <wrl.h>

struct ShaderResource {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> cbuffer;
	size_t count, size;
};

class StackAlloc {
	size_t mappedConstantBufferOffset_ = 0;
	ID3D12Device* device_;
public:
	ShaderResource resource_;
	D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress_;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle_;
	UINT8* mappedConstantBuffer_ = nullptr;
	UINT cbvDescriptorSize_;
	struct FrameDesc;
private:
	std::vector<FrameDesc> frames_;
	unsigned short Push(UINT size, unsigned short count);
public:
	struct FrameDesc {
		size_t alignedConstantBufferSize;
		UINT8* destination;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	};
	StackAlloc(ID3D12Device* device, size_t count, size_t size);
	~StackAlloc();
	template<typename T>
	unsigned short Push(unsigned short count) { return Push(UINT(sizeof(T)), count); }
	const FrameDesc& Get(size_t index);
	void Pop(size_t count) { /* TODO:: cbvCpuHandle.Offset(count * -cbvDescriptorSize_);*/}
};
struct ShaderResources {
	StackAlloc staticResources_;
	ShaderResources(ID3D12Device* device);
};
