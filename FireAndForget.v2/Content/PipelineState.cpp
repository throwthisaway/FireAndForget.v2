#include "pch.h"
#include "d3dcompiler.h"
#include "PipelineState.h"
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
using namespace Microsoft::WRL;
using namespace concurrency;

namespace {
	/*const D3D12_INPUT_ELEMENT_DESC debugInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };*/
	const D3D12_INPUT_ELEMENT_DESC pnLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }};
	const D3D12_INPUT_ELEMENT_DESC pntLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }};
	const D3D12_INPUT_ELEMENT_DESC ptLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };
	const D3D12_INPUT_ELEMENT_DESC fsQuadLayout[] = {
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

PipelineStates::PipelineStates(const DX::DeviceResources* deviceResources) :
	deviceResources_(deviceResources) {
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = _countof(deferredRTFmts);
			for (UINT i = 0; i < state.NumRenderTargets; ++i)
				state.RTVFormats[i] = deferredRTFmts[i];
			state.DSVFormat = deviceResources->GetDepthBufferFormat();
			state.SampleDesc.Count = 1;
			deferred = state;
		}
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = {};
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = deviceResources->GetBackBufferFormat();
			state.DSVFormat = DXGI_FORMAT_UNKNOWN;
			state.SampleDesc.Count = 1;
			post = state;
		}

		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = deviceResources->GetBackBufferFormat();
			state.DSVFormat = deviceResources->GetDepthBufferFormat();
			state.SampleDesc.Count = 1;
			forward = state;
		}

		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = {};
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = DXGI_FORMAT_D32_FLOAT;
			state.DSVFormat = DXGI_FORMAT_UNKNOWN;
			state.SampleDesc.Count = 1;
			depth = state;
		}

		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = {};
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
			state.DSVFormat = DXGI_FORMAT_UNKNOWN;
			state.SampleDesc.Count = 1;
			rg16 = state;
		}

		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = {};
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			state.DSVFormat = DXGI_FORMAT_UNKNOWN;
			state.SampleDesc.Count = 1;
			pre = state;
		}

}

PipelineStates::~PipelineStates() {}

void PipelineStates::CreateDeviceDependentResources() {

	rootSignatures_.resize(ROOT_SIG_COUNT);
	states_.resize(ShaderStructures::Count);
	auto d3dDevice = deviceResources_->GetD3DDevice();
	{
		// ROOT_VS_1CB_PS_1CB
		CD3DX12_ROOT_PARAMETER parameter[2];
		CD3DX12_DESCRIPTOR_RANGE range0;
		std::vector<UINT> ranges{ 1, 1 };
		range0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter[0].InitAsDescriptorTable(ranges[0], &range0, D3D12_SHADER_VISIBILITY_VERTEX);

		CD3DX12_DESCRIPTOR_RANGE range1;
		range1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter[1].InitAsDescriptorTable(ranges[1], &range1, D3D12_SHADER_VISIBILITY_PIXEL);

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
		std::vector<UINT> ranges{ 1, 2 };
		range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter[0].InitAsDescriptorTable(ranges[0], range, D3D12_SHADER_VISIBILITY_VERTEX);
		range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter[1].InitAsDescriptorTable(ranges[1], range + 1, D3D12_SHADER_VISIBILITY_PIXEL);

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

	shaderTasks.push_back(CreateShader(ShaderStructures::Pos, ROOT_VS_1CB_PS_1CB, L"PosVS.cso", L"PosPS.cso", { pnLayout, _countof(pnLayout) }, deferred));
	shaderTasks.push_back(CreateShader(ShaderStructures::Tex, ROOT_VS_1CB_PS_1TX_1CB, L"TexVS.cso", L"TexPS.cso", { pntLayout, _countof(pntLayout) }, deferred));
	shaderTasks.push_back(CreateShader(ShaderStructures::Debug, ROOT_VS_1CB_PS_1CB, L"DebugVS.cso", L"DebugPS.cso", { pnLayout, _countof(pnLayout) }, deferred));
	shaderTasks.push_back(CreateShader(ShaderStructures::Deferred, ROOT_UNKNOWN, L"FSQuadVS.cso", L"DeferredPS.cso", { fsQuadLayout, _countof(fsQuadLayout) }, post));
	shaderTasks.push_back(CreateShader(ShaderStructures::DeferredPBR, ROOT_UNKNOWN, L"FSQuadVS.cso", L"DeferredPBRPS.cso", {fsQuadLayout, _countof(fsQuadLayout) }, post));

	shaderTasks.push_back(CreateShader(ShaderStructures::CubeEnvMap, ROOT_UNKNOWN, L"FSQuadVS.cso", L"CubeEnvMapPS.cso", {fsQuadLayout, _countof(fsQuadLayout) }, pre));
	shaderTasks.push_back(CreateShader(ShaderStructures::Bg, ROOT_UNKNOWN, L"BgVS.cso", L"BgPS.cso", {fsQuadLayout, _countof(fsQuadLayout) }, forward));
	shaderTasks.push_back(CreateShader(ShaderStructures::Irradiance, ROOT_UNKNOWN, L"CubeEnvMapVS.cso", L"CubeEnvIrPS.cso", {fsQuadLayout, _countof(fsQuadLayout) }, pre));
	shaderTasks.push_back(CreateShader(ShaderStructures::PrefilterEnv, ROOT_UNKNOWN, L"CubeEnvMapVS.cso", L"CubeEnvPrefilterPS.cso", {fsQuadLayout, _countof(fsQuadLayout) }, pre));
	shaderTasks.push_back(CreateShader(ShaderStructures::BRDFLUT, ROOT_UNKNOWN, L"FSQuadVS.cso", L"BRDFLUTPS.cso", {fsQuadLayout, _countof(fsQuadLayout) }, rg16));
	shaderTasks.push_back(CreateShader(ShaderStructures::Downsample, ROOT_UNKNOWN, L"FSQuadVS.cso", L"DownsampleDepthPS.cso", {fsQuadLayout, _countof(fsQuadLayout) }, depth));
	completionTask_ = Concurrency::when_all(std::begin(shaderTasks), std::end(shaderTasks)).then([this]() { shaderTasks.clear(); });;
}

Concurrency::task<void> PipelineStates::CreateShader(ShaderId id,
		size_t rootSignatureIndex,
		const wchar_t* vs, 
		const wchar_t* ps, 
		const D3D12_INPUT_LAYOUT_DESC il,
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
	auto vsTask = DX::ReadDataAsync(vs).then([](std::vector<byte>& data) {
		return std::make_shared<std::vector<byte>>(std::move(data));
	});
	auto psTask = DX::ReadDataAsync(ps).then([](std::vector<byte>& data) {
		return std::make_shared<std::vector<byte>>(std::move(data));
	});
	return (vsTask && psTask).then([=](const std::vector<std::shared_ptr<std::vector<byte>>>& res) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = desc;
		state.InputLayout = il;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		if (rootSignatureIndex == ROOT_UNKNOWN) {
			ComPtr<ID3DBlob> blob;
			// rs from ps
			::D3DGetBlobPart(res[1]->data(), res[1]->size(), D3D_BLOB_ROOT_SIGNATURE, 0, blob.GetAddressOf());
			DX::ThrowIfFailed(deviceResources_->GetD3DDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
			state.pRootSignature = rootSignature.Get();
			rootSignatures_.push_back(rootSignature);
		} else {
			rootSignature = rootSignatures_[rootSignatureIndex];
			state.pRootSignature =rootSignature.Get();
		}
		state.VS = CD3DX12_SHADER_BYTECODE(&res[0]->front(), res[0]->size());
		state.PS = CD3DX12_SHADER_BYTECODE(&res[1]->front(), res[1]->size());
		ComPtr<ID3D12PipelineState> pipelineState;
		DX::ThrowIfFailed(deviceResources_->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipelineState)));
		states_[id] = { rootSignature, pipelineState, false};
	});
}