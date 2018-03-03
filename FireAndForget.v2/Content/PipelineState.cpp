#include "pch.h"
#include "PipelineState.h"
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
#include "..\Content\ShaderStructures.h"
#include "..\source\cpp\ShaderStructures.h"
using namespace Microsoft::WRL;
using namespace concurrency;

namespace {
	//PipelineStates::CBuffer SetupDescriptorHeap(ID3D12Device* d3dDevice, ID3D12DescriptorHeap* cbvHeap, ID3D12Resource* constantBuffer, size_t size, size_t numDesc, size_t repeatCount, size_t indexOffset) {
	//	const UINT c_alignedConstantBufferSize = (size + 255) & ~255;
	//	// Create constant buffer views to access the upload buffer.
	//	D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = constantBuffer->GetGPUVirtualAddress();
	//	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart());
	//	UINT cbvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//	cbvCpuHandle.Offset(cbvDescriptorSize * indexOffset);
	//	for (int n = 0; n < repeatCount; ++n)
	//	{
	//		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	//		desc.BufferLocation = cbvGpuAddress;
	//		desc.SizeInBytes = c_alignedConstantBufferSize;
	//		d3dDevice->CreateConstantBufferView(&desc, cbvCpuHandle);

	//		cbvGpuAddress += desc.SizeInBytes;
	//		cbvCpuHandle.Offset(cbvDescriptorSize * numDesc);
	//	}

	//	return { constantBuffer, cbvDescriptorSize, c_alignedConstantBufferSize, size };
	//}

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

	rootSignatures_.resize(ROOT_SIG_COUNT);
	states_.resize(ShaderStructures::Count);
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
		rootSignatures_[ROOT_VS_1CB] = rootSignature;
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
		rootSignatures_[ROOT_VS_1CB_PS_1CB] = rootSignature;
	}
	
	{
		// ROOT_VS_1CB_PS_1TX_2CB
		CD3DX12_ROOT_PARAMETER parameter[2];
		CD3DX12_DESCRIPTOR_RANGE range[3];
		range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter[0].InitAsDescriptorTable(1, range, D3D12_SHADER_VISIBILITY_VERTEX);
		range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0);
		parameter[1].InitAsDescriptorTable(2, range + 1, D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(_ARRAYSIZE(parameter), parameter, 1/*1 static sampler*/, &sampler, rootSignatureFlags);

		ComPtr<ID3DBlob> pSignature;
		ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		ComPtr<ID3D12RootSignature>	rootSignature;
		DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
		NAME_D3D12_OBJECT(rootSignature);
		rootSignatures_[ROOT_VS_1CB_PS_1TX_2CB] = rootSignature;
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
			//state.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = deviceResources_->GetBackBufferFormat();
			state.DSVFormat = deviceResources_->GetDepthBufferFormat();
			state.SampleDesc.Count = 1;

			DX::ThrowIfFailed(deviceResources_->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipelineState)));
			states_[ShaderStructures::ColPos] = { rootSignatureIndex, pipelineState/*, [](ID3D12Device* device, unsigned short repeat) {
				return CreateShaderResources<FireAndForget_v2::ModelViewProjectionConstantBuffer>(device, repeat); } */};
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
			state.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = deviceResources_->GetBackBufferFormat();
			state.DSVFormat = deviceResources_->GetDepthBufferFormat();
			state.SampleDesc.Count = 1;

			DX::ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipelineState)));
			states_[ShaderStructures::Pos] = { rootSignatureIndex, pipelineState/*, [](ID3D12Device* device, unsigned short repeat) {
				return CreateShaderResources<Materials::cMVP, Materials::cColor>(device, repeat); }*/ };
			//pos_.pipelineIndex = states_.size() - 1;
		});
	}

	task<void> createTexPipelineStateTask;
	{
		std::shared_ptr<std::vector<byte>> vertexShader = std::make_shared<std::vector<byte>>(),
			pixelShader = std::make_shared<std::vector<byte>>();
		auto createVSTask = DX::ReadDataAsync(L"TexVS.cso").then([this, vertexShader](std::vector<byte>& fileData) mutable {
			*vertexShader = std::move(fileData);
		});
		auto createPSTask = DX::ReadDataAsync(L"TexPS.cso").then([this, pixelShader](std::vector<byte>& fileData) mutable {
			*pixelShader = std::move(fileData);
		});

		createTexPipelineStateTask = (createPSTask && createVSTask).then([this, d3dDevice, vertexShader, pixelShader]() mutable {
			ComPtr<ID3D12PipelineState> pipelineState;
			auto rootSignatureIndex = ROOT_VS_1CB_PS_1TX_2CB;
			const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.InputLayout = { inputLayout, _countof(inputLayout) };
			state.pRootSignature = rootSignatures_[rootSignatureIndex].Get();
			state.VS = CD3DX12_SHADER_BYTECODE(&vertexShader->front(), vertexShader->size());
			state.PS = CD3DX12_SHADER_BYTECODE(&pixelShader->front(), pixelShader->size());
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = deviceResources_->GetBackBufferFormat();
			state.DSVFormat = deviceResources_->GetDepthBufferFormat();
			state.SampleDesc.Count = 1;

			DX::ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipelineState)));
			states_[ShaderStructures::Tex] = { rootSignatureIndex, pipelineState };
		});
	}
	completionTask_ = (createColPosPipelineStateTask && createPosPipelineStateTask && createTexPipelineStateTask).then([this]() {
		loadingComplete_ = true;
	});
}
