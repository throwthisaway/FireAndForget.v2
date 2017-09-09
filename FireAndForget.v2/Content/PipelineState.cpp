#include "pch.h"
#include "PipelineState.h"
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
#include "..\Content\ShaderStructures.h"
#include "..\source\cpp\Materials.h"
using namespace Microsoft::WRL;
using namespace concurrency;

namespace {
	auto CreateDescriptorHeapForCBuffer(ID3D12Device* d3dDevice, size_t numDesc) {
		ComPtr<ID3D12DescriptorHeap> cbvHeap;
		// Create a descriptor heap for the constant buffers.
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = DX::c_frameCount * numDesc;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbvHeap)));

		NAME_D3D12_OBJECT(cbvHeap);
		return cbvHeap;
	}
	PipelineStates::CBuffer CreateConstantBuffers(ID3D12Device* d3dDevice, ID3D12DescriptorHeap* cbvHeap, size_t size, size_t numDesc, size_t indexOffset) {
		// Constant buffers must be 256-byte aligned.
		const UINT c_alignedConstantBufferSize = (size + 255) & ~255;
		ComPtr<ID3D12Resource> constantBuffer;
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(DX::c_frameCount * c_alignedConstantBufferSize);
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&constantBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBuffer)));

		NAME_D3D12_OBJECT(constantBuffer);

		// Create constant buffer views to access the upload buffer.
		D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = constantBuffer->GetGPUVirtualAddress();
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart());
		UINT cbvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cbvCpuHandle.Offset(cbvDescriptorSize * indexOffset);
		for (int n = 0; n < DX::c_frameCount; ++n)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
			desc.BufferLocation = cbvGpuAddress;
			desc.SizeInBytes = c_alignedConstantBufferSize;
			d3dDevice->CreateConstantBufferView(&desc, cbvCpuHandle);

			cbvGpuAddress += desc.SizeInBytes;
			cbvCpuHandle.Offset(cbvDescriptorSize * numDesc);
		}

		return { constantBuffer, cbvDescriptorSize, c_alignedConstantBufferSize, size };
	}
}
UINT8* PipelineStates::CBuffer::Map() {
	// Map the constant buffers.
	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	UINT8* mappedConstantBuffer = nullptr;
	DX::ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedConstantBuffer)));
	//ZeroMemory(mappedConstantBuffer, DX::c_frameCount * alignedConstantBufferSize);
	return mappedConstantBuffer;
}
void PipelineStates::CBuffer::Unmap() {
	constantBuffer->Unmap(0, nullptr);
}
PipelineStates::CBuffer::~CBuffer() {
	//Unmap();
}
PipelineStates::PipelineStates(const DX::DeviceResources* deviceResources) :
	deviceResources_(deviceResources) {}

PipelineStates::~PipelineStates() {}

void PipelineStates::CreateDeviceDependentResources() {

	auto d3dDevice = deviceResources_->GetD3DDevice();
	{
		//ROOT_VS_1CB
		CD3DX12_DESCRIPTOR_RANGE range;
		CD3DX12_ROOT_PARAMETER parameter;

		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(1, &parameter, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> pSignature;
		ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		ComPtr<ID3D12RootSignature>	rootSignature;
		DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
		NAME_D3D12_OBJECT(rootSignature);
		rootSignatures_.push_back(rootSignature);
	}

	{
		// ROOT_VS_1CB_PS_1CB
		CD3DX12_ROOT_PARAMETER parameter[2];
		CD3DX12_DESCRIPTOR_RANGE range0;
		range0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter[0].InitAsDescriptorTable(1, &range0, D3D12_SHADER_VISIBILITY_VERTEX);

		CD3DX12_DESCRIPTOR_RANGE range1;
		range1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter[1].InitAsDescriptorTable(1, &range1, D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(_ARRAYSIZE(parameter), parameter, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> pSignature;
		ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		ComPtr<ID3D12RootSignature>	rootSignature;
		DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
		NAME_D3D12_OBJECT(rootSignature);
		rootSignatures_.push_back(rootSignature);
	}
	
	task<void> createColPosPipelineStateTask;
	{
		std::shared_ptr<std::vector<byte>> vertexShader = std::make_shared<std::vector<byte>>(),
			pixelShader = std::make_shared<std::vector<byte>>();
		auto createVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso").then([this, vertexShader](std::vector<byte>& fileData) mutable {
			*vertexShader = std::move(fileData);
		});
		auto createPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso").then([this, pixelShader](std::vector<byte>& fileData) mutable {
			*pixelShader = std::move(fileData);
		});

		createColPosPipelineStateTask = (createPSTask && createVSTask).then([this, d3dDevice, vertexShader, pixelShader]() mutable {
			ComPtr<ID3D12PipelineState> pipelineState;
			auto rootSignatureIndex = ROOT_VS_1CB;
			 const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.InputLayout = { inputLayout, _countof(inputLayout) };
			state.pRootSignature = rootSignatures_[rootSignatureIndex].Get();
			state.VS = CD3DX12_SHADER_BYTECODE(&vertexShader->front(), vertexShader->size());
			state.PS = CD3DX12_SHADER_BYTECODE(&pixelShader->front(), pixelShader->size());
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = deviceResources_->GetBackBufferFormat();
			state.DSVFormat = deviceResources_->GetDepthBufferFormat();
			state.SampleDesc.Count = 1;
			
			DX::ThrowIfFailed(deviceResources_->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipelineState)));
			auto cbvHeap = CreateDescriptorHeapForCBuffer(d3dDevice, 1);
			states_.push_back({ rootSignatureIndex, pipelineState, cbvHeap, { CreateConstantBuffers(d3dDevice, cbvHeap.Get(), sizeof(FireAndForget_v2::ModelViewProjectionConstantBuffer), 1, 0) } });
		});
	}

	task<void> createPosPipelineStateTask;
	{
		std::shared_ptr<std::vector<byte>> vertexShader = std::make_shared<std::vector<byte>>(),
			pixelShader = std::make_shared<std::vector<byte>>();
		auto createVSTask = DX::ReadDataAsync(L"PosVS.cso").then([this, vertexShader](std::vector<byte>& fileData) mutable {
			*vertexShader = std::move(fileData);
		});
		auto createPSTask = DX::ReadDataAsync(L"PosPS.cso").then([this, pixelShader](std::vector<byte>& fileData) mutable {
			*pixelShader = std::move(fileData);
		});

		createPosPipelineStateTask = (createPSTask && createVSTask).then([this, d3dDevice, vertexShader, pixelShader]() mutable {
			ComPtr<ID3D12PipelineState> pipelineState;
			auto rootSignatureIndex = ROOT_VS_1CB_PS_1CB;
			const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }	};

			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.InputLayout = { inputLayout, _countof(inputLayout) };
			state.pRootSignature = rootSignatures_[rootSignatureIndex].Get();
			state.VS = CD3DX12_SHADER_BYTECODE(&vertexShader->front(), vertexShader->size());
			state.PS = CD3DX12_SHADER_BYTECODE(&pixelShader->front(), pixelShader->size());
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = deviceResources_->GetBackBufferFormat();
			state.DSVFormat = deviceResources_->GetDepthBufferFormat();
			state.SampleDesc.Count = 1;

			const size_t cbufferCount = 2;
			DX::ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipelineState)));
			auto cbvHeap = CreateDescriptorHeapForCBuffer(d3dDevice, cbufferCount);
			auto cbufferVS = CreateConstantBuffers(d3dDevice, cbvHeap.Get(), sizeof(Materials::cMVP), cbufferCount, 0);
			auto cbufferPS = CreateConstantBuffers(d3dDevice, cbvHeap.Get(), sizeof(Materials::cColor), cbufferCount, 1);
			states_.push_back({ rootSignatureIndex, pipelineState, cbvHeap, {cbufferVS, cbufferPS } });
		});
	}

	completionTask_ = (createColPosPipelineStateTask && createPosPipelineStateTask).then([this]() {
		loadingComplete_ = true;
	});
}
