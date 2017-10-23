#include "pch.h"
#include "DescriptorHeapAllocator.h"
#include "..\Common\DirectXHelper.h"
using namespace Microsoft::WRL;

namespace {
	const size_t staticDescCount = 256, staticBufferSize = 65536;

	auto CreateDescriptorHeapForCBuffer(ID3D12Device* d3dDevice, UINT numDesc) {
		ComPtr<ID3D12DescriptorHeap> cbvHeap;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = numDesc;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbvHeap)));

		NAME_D3D12_OBJECT(cbvHeap);
		return cbvHeap;
	}

	ComPtr<ID3D12Resource> CreateConstantBuffer(ID3D12Device* d3dDevice, size_t size) {
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

		NAME_D3D12_OBJECT(constantBuffer);
		return constantBuffer;
	}

	//https://stackoverflow.com/questions/12024304/c11-number-of-variadic-template-function-parameters

	template<size_t ...>
	struct SizeOf : std::integral_constant<std::size_t, 0> {};

	template<size_t Size0, size_t... SizeX>
	struct SizeOf<Size0, SizeX...> : std::integral_constant<std::size_t, Size0 + SizeOf<SizeX...>::value > {};

	template<typename T> constexpr T AlignTo256(T val) { return (val + 255) & ~255; }
	// Constant buffers must be 256-byte aligned.
	template<size_t ...>
	struct Aligned256SizeOf : std::integral_constant<std::size_t, 0> {};

	template<size_t Size0, size_t... SizeX>
	struct Aligned256SizeOf<Size0, SizeX...> : std::integral_constant<std::size_t, AlignTo256(Size0) + Aligned256SizeOf<SizeX...>::value > {};

	//template<typename... BuffersT>
	//struct ShaderResourceDesc {};

	template <typename...>
	void SetupDescriptorHeapInternal(ID3D12Device* device, D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress, CD3DX12_CPU_DESCRIPTOR_HANDLE& cbvCpuHandle) {}

	template <typename BufferT, typename... BuffersT>
	void SetupDescriptorHeapInternal(ID3D12Device* device, D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress, CD3DX12_CPU_DESCRIPTOR_HANDLE& cbvCpuHandle) {
		UINT cbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		const UINT c_alignedConstantBufferSize = AlignTo256(sizeof(BufferT));
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = cbvGpuAddress;
		desc.SizeInBytes = c_alignedConstantBufferSize;
		device->CreateConstantBufferView(&desc, cbvCpuHandle);
		cbvGpuAddress += desc.SizeInBytes;
		cbvCpuHandle.Offset(cbvDescriptorSize);
		SetupDescriptorHeapInternal<BuffersT...>(device, cbvGpuAddress, cbvCpuHandle);
	}

	template <typename... BuffersT>
	void SetupDescriptorHeap(ID3D12Device* device, ID3D12DescriptorHeap* cbvHeap, ID3D12Resource* constantBuffer, unsigned short repeat) {
		D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = constantBuffer->GetGPUVirtualAddress();
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart());
		for (unsigned short n = 0; n < repeat; ++n) {
			SetupDescriptorHeapInternal(device, cbvGpuAddress, cbvCpuHandle);
		}
	}
	template <typename... BuffersT>
	ShaderResource CreateShaderResources(ID3D12Device* device, unsigned short repeat) {
		ShaderResource res;
		res.cbvHeap = CreateDescriptorHeapForCBuffer(device, sizeof...(BuffersT)* repeat);
		res.cbuffer = CreateConstantBuffer(device, Aligned256SizeOf<sizeof(BuffersT)...>::value * repeat);
		SetupDescriptorHeap<BuffersT...>(device, res.cbvHeap.Get(), res.cbuffer.Get(), repeat);
		return res;
	}
}
ShaderResource CreateShaderResource(ID3D12Device* device, UINT count, size_t size) {
	return { CreateDescriptorHeapForCBuffer(device, count), CreateConstantBuffer(device, size), count, size };
}
StackAlloc::StackAlloc(ID3D12Device* device, size_t count, size_t size) :
	device_(device),
	resource_(CreateShaderResource(device, staticDescCount, staticBufferSize)),
	cbvGpuAddress_(resource_.cbuffer->GetGPUVirtualAddress()),
	cbvCpuHandle_(resource_.cbvHeap->GetCPUDescriptorHandleForHeapStart()),
	cbvDescriptorSize_(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)) {
	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	DX::ThrowIfFailed(resource_.cbuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedConstantBuffer_)));
}
unsigned short StackAlloc::Push(UINT size, unsigned short count) {
	auto alignedSize = AlignTo256(size);
#undef max
	if (alignedSize * count > resource_.size) return std::numeric_limits<unsigned short>::max();
	size_t index = frames_.size();
	for (size_t i = 0; i < count; ++i) {
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = cbvGpuAddress_;
		desc.SizeInBytes = alignedSize;
		device_->CreateConstantBufferView(&desc, cbvCpuHandle_);

		UINT8* destination = mappedConstantBufferOffset_ + mappedConstantBuffer_;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(resource_.cbvHeap->GetGPUDescriptorHandleForHeapStart(), INT(frames_.size()), cbvDescriptorSize_);
		frames_.push_back({ alignedSize, destination, gpuHandle });

		cbvGpuAddress_ += desc.SizeInBytes;
		mappedConstantBufferOffset_ += desc.SizeInBytes;
		cbvCpuHandle_.Offset(cbvDescriptorSize_);
	}
	return unsigned short(index);
}
StackAlloc::~StackAlloc() {
	resource_.cbuffer->Unmap(0, nullptr);
}
const StackAlloc::FrameDesc& StackAlloc::Get(size_t index) {
	assert(frames_.size() > index);
	return frames_[index];
}

ShaderResources::ShaderResources(ID3D12Device* device) : staticResources_(device, staticDescCount, staticBufferSize) {}
