#include "pch.h"
#include "PipelineState.h"
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
#include "..\Content\ShaderStructures.h"
using namespace Microsoft::WRL;
using namespace concurrency;

namespace {
	const D3D12_INPUT_ELEMENT_DESC debugInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };
	const D3D12_INPUT_ELEMENT_DESC posInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }};
	const D3D12_INPUT_ELEMENT_DESC texInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }};
	const D3D12_INPUT_ELEMENT_DESC deferredInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };
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
		// ROOT_VS_1CB_PS_1TX_1CB
		CD3DX12_ROOT_PARAMETER parameter[2];
		CD3DX12_DESCRIPTOR_RANGE range[3];
		range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter[0].InitAsDescriptorTable(1, range, D3D12_SHADER_VISIBILITY_VERTEX);
		range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
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
		rootSignatures_[ROOT_VS_1CB_PS_1TX_1CB] = rootSignature;
	}



	shaderTasks_.push_back(CreateShader(ShaderStructures::Pos, ROOT_VS_1CB_PS_1CB, L"PosVS.cso", L"PosPS.cso", posInputLayout, _countof(posInputLayout)));
	shaderTasks_.push_back(CreateShader(ShaderStructures::Tex, ROOT_VS_1CB_PS_1TX_1CB, L"TexVS.cso", L"TexPS.cso", texInputLayout, _countof(texInputLayout)));
	shaderTasks_.push_back(CreateShader(ShaderStructures::Debug, ROOT_VS_1CB_PS_1CB, L"DebugVS.cso", L"DebugPS.cso", debugInputLayout, _countof(debugInputLayout)));
	shaderTasks_.push_back(CreateDeferredShader(ShaderStructures::Deferred, L"DeferredVS.cso", L"DeferredPS.cso", deferredInputLayout, _countof(deferredInputLayout)));
	// TODO:: hack!!! write DeferredPBR
	shaderTasks_.push_back(CreateDeferredShader(ShaderStructures::DeferredPBR, L"DeferredVS.cso", L"DeferredPS.cso", deferredInputLayout, _countof(deferredInputLayout)));
	shaderTasks_.push_back(DX::ReadDataAsync(L"DeferredRootSig.cso").then([this](std::vector<byte>& fileData) mutable {
		ComPtr<ID3D12RootSignature>	rootSignature;
		DX::ThrowIfFailed(deviceResources_->GetD3DDevice()->CreateRootSignature(0, fileData.data(), fileData.size(), IID_PPV_ARGS(&rootSignature)));
		NAME_D3D12_OBJECT(rootSignature);
		rootSignatures_[ROOT_VS_0CB_PS_1CB_5TX] = rootSignature;
	}));
	completionTask_ = Concurrency::when_all(std::begin(shaderTasks_), std::end(shaderTasks_)).then([this]() {
		shaderTasks_.clear();
		loadingComplete_ = true;
	});
}

Concurrency::task<void> PipelineStates::CreateShader(ShaderStructures::ShaderId id, size_t rootSignatureIndex, const wchar_t* vs, const wchar_t* ps, const D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT count) {
	std::shared_ptr<std::vector<byte>> vertexShader = std::make_shared<std::vector<byte>>(),
		pixelShader = std::make_shared<std::vector<byte>>();
	auto createVSTask = DX::ReadDataAsync(vs).then([vertexShader](std::vector<byte>& fileData) mutable {
		*vertexShader = std::move(fileData);
	});
	auto createPSTask = DX::ReadDataAsync(ps).then([pixelShader](std::vector<byte>& fileData) mutable {
		*pixelShader = std::move(fileData);
	});

	return (createPSTask && createVSTask).then([this, id, rootSignatureIndex, inputLayout, count, vertexShader, pixelShader]() mutable {
		ComPtr<ID3D12PipelineState> pipelineState;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
		state.InputLayout = { inputLayout, count };
		state.pRootSignature = rootSignatures_[rootSignatureIndex].Get();
		state.VS = CD3DX12_SHADER_BYTECODE(&vertexShader->front(), vertexShader->size());
		state.PS = CD3DX12_SHADER_BYTECODE(&pixelShader->front(), pixelShader->size());
		state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		state.SampleMask = UINT_MAX;
		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		state.NumRenderTargets = ShaderStructures::RenderTargetCount;
		for (int i = 0; i < ShaderStructures::RenderTargetCount; ++i)
			state.RTVFormats[i] = renderTargetFormats[i];
		state.DSVFormat = deviceResources_->GetDepthBufferFormat();
		state.SampleDesc.Count = 1;

		DX::ThrowIfFailed(deviceResources_->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipelineState)));
		states_[id] = { rootSignatureIndex, pipelineState, false };
	});
}
Concurrency::task<void> PipelineStates::CreateDeferredShader(ShaderStructures::ShaderId id, const wchar_t* vs, const wchar_t* ps, const D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT count) {
	std::shared_ptr<std::vector<byte>> vertexShader = std::make_shared<std::vector<byte>>(),
		pixelShader = std::make_shared<std::vector<byte>>();
	auto createVSTask = DX::ReadDataAsync(vs).then([vertexShader](std::vector<byte>& fileData) mutable {
		*vertexShader = std::move(fileData);
	});
	auto createPSTask = DX::ReadDataAsync(ps).then([pixelShader](std::vector<byte>& fileData) mutable {
		*pixelShader = std::move(fileData);
	});

	return (createPSTask && createVSTask).then([this, id, inputLayout, count, vertexShader, pixelShader]() mutable {
		ComPtr<ID3D12PipelineState> pipelineState;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
		state.InputLayout = { inputLayout, count };
		// compiled separately referenced in shader
		//state.pRootSignature = rootSignatures_[ROOT_VS_0CB_PS_1CB_5TX].Get();
		state.VS = CD3DX12_SHADER_BYTECODE(&vertexShader->front(), vertexShader->size());
		state.PS = CD3DX12_SHADER_BYTECODE(&pixelShader->front(), pixelShader->size());
		state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		state.DepthStencilState = {};
		state.SampleMask = UINT_MAX;
		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		state.NumRenderTargets = 1;
		state.RTVFormats[0] = deviceResources_->GetBackBufferFormat();
		state.DSVFormat = deviceResources_->GetDepthBufferFormat();
		state.SampleDesc.Count = 1;

		DX::ThrowIfFailed(deviceResources_->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipelineState)));
		states_[id] = { ROOT_VS_0CB_PS_1CB_5TX, pipelineState, true};
	});
}