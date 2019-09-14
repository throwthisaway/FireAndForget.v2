#include "pch.h"
#include "d3dcompiler.h"
#include "PipelineState.h"
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
#include <string>
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
	/*const D3D12_INPUT_ELEMENT_DESC fsQuadLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };*/
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

PipelineStates::PipelineStates(ID3D12Device* device, DXGI_FORMAT backbufferFormat, DXGI_FORMAT depthbufferFormat) :
	device(device) {
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
			state.DSVFormat = depthbufferFormat;
			state.SampleDesc.Count = 1;
			geometry = state;
		}
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			D3D12_BLEND_DESC desc = {};
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].BlendOp = desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlend = desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			state.BlendState = desc;
			state.DepthStencilState = {};
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = backbufferFormat;
			state.DSVFormat = DXGI_FORMAT_UNKNOWN;
			state.SampleDesc.Count = 1;
			lighting = state;
		}

		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // less AND equal bc. of bg. shader xyww trick, depth write/test should jut be turned off instead
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 1;
			state.RTVFormats[0] = backbufferFormat;
			state.DSVFormat = depthbufferFormat;
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
			state.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
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

		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = {};
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 2;
			state.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
			state.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT; // debug
			state.DSVFormat = DXGI_FORMAT_UNKNOWN;
			state.SampleDesc.Count = 1;
			ssao = state;
		}
}

PipelineStates::~PipelineStates() {}

void PipelineStates::CreateDeviceDependentResources() {

	rootSignatures_.resize(ROOT_SIG_COUNT);
	states_.resize(ShaderStructures::Count);
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
		DX::ThrowIfFailed(device->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
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
		DX::ThrowIfFailed(device->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
		NAME_D3D12_OBJECT(rootSignature);
		rootSignatures_[ROOT_VS_1CB_PS_1TX_1CB] = rootSignature;
	}

	shaderTasks.push_back(CreateShader(ShaderStructures::Pos, ROOT_VS_1CB_PS_1CB, L"PosVS.cso", L"PosPS.cso", nullptr, { pnLayout, _countof(pnLayout) }, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(ShaderStructures::Tex, ROOT_VS_1CB_PS_1TX_1CB, L"TexVS.cso", L"TexPS.cso", nullptr, { pntLayout, _countof(pntLayout) }, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(ShaderStructures::Debug, ROOT_VS_1CB_PS_1CB, L"DebugVS.cso", L"DebugPS.cso", nullptr, { pnLayout, _countof(pnLayout) }, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(ShaderStructures::Deferred, ROOT_UNKNOWN, L"FSQuadVS.cso", L"DeferredPS.cso", nullptr, { {}, 0 }, lighting, State::RenderPass::Lighting));
	shaderTasks.push_back(CreateShader(ShaderStructures::DeferredPBR, ROOT_UNKNOWN, L"FSQuadVS.cso", L"DeferredPBRPS.cso", nullptr, { {}, 0 }, lighting, State::RenderPass::Lighting));

	shaderTasks.push_back(CreateShader(ShaderStructures::CubeEnvMap, ROOT_UNKNOWN, L"CubeEnvMapVS.cso", L"CubeEnvMapPS.cso", nullptr, {pnLayout, _countof(pnLayout) }, pre, State::RenderPass::Pre));
	shaderTasks.push_back(CreateShader(ShaderStructures::Bg, ROOT_UNKNOWN, L"BgVS.cso", L"BgPS.cso", nullptr, {pnLayout, _countof(pnLayout) }, forward, State::RenderPass::Forward));
	shaderTasks.push_back(CreateShader(ShaderStructures::Irradiance, ROOT_UNKNOWN, L"CubeEnvMapVS.cso", L"CubeEnvIrPS.cso", nullptr, {pnLayout, _countof(pnLayout) }, pre, State::RenderPass::Pre));
	shaderTasks.push_back(CreateShader(ShaderStructures::PrefilterEnv, ROOT_UNKNOWN, L"CubeEnvMapVS.cso", L"CubeEnvPrefilterPS.cso", nullptr, {pnLayout, _countof(pnLayout) }, pre, State::RenderPass::Pre));
	shaderTasks.push_back(CreateShader(ShaderStructures::BRDFLUT, ROOT_UNKNOWN, L"FSQuadVS.cso", L"BRDFLUTPS.cso", nullptr, { {}, 0 }, rg16, State::RenderPass::Pre));
	shaderTasks.push_back(CreateShader(ShaderStructures::Downsample, ROOT_UNKNOWN, L"FSQuadVS.cso", L"DownsampleDepthPS.cso", nullptr, { {}, 0 }, depth, State::RenderPass::Lighting));
	shaderTasks.push_back(CreateComputeShader(ShaderStructures::GenMips, ROOT_UNKNOWN, L"GenMips.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(ShaderStructures::GenMipsOddX, ROOT_UNKNOWN, L"GenMipsOddX.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(ShaderStructures::GenMipsOddY, ROOT_UNKNOWN, L"GenMipsOddY.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(ShaderStructures::GenMipsOddXOddY, ROOT_UNKNOWN, L"GenMipsOddXOddY.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(ShaderStructures::GenMipsSRGB, ROOT_UNKNOWN, L"GenMipsSRGB.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(ShaderStructures::GenMipsOddXSRGB, ROOT_UNKNOWN, L"GenMipsOddXSRGB.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(ShaderStructures::GenMipsOddYSRGB, ROOT_UNKNOWN, L"GenMipsOddYSRGB.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(ShaderStructures::GenMipsOddXOddYSRGB, ROOT_UNKNOWN, L"GenMipsOddXOddYSRGB.cso", State::RenderPass::Pre));
#if 1
	shaderTasks.push_back(CreateShader(ShaderStructures::ModoDN, ROOT_UNKNOWN, L"ModoDNVS.cso", L"ModoDNPS.cso", L"TangentGS.cso", { pntLayout, _countof(pntLayout) }, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(ShaderStructures::ModoDNMR, ROOT_UNKNOWN, L"ModoDNVS.cso", L"ModoDNMRPS.cso", L"TangentGS.cso", { pntLayout, _countof(pntLayout) }, geometry, State::RenderPass::Geometry));
#else
	shaderTasks.push_back(CreateShader(ShaderStructures::ModoDN, ROOT_UNKNOWN, L"ModoDNVS.cso", L"ModoDNPS.cso", nullptr, { pntLayout, _countof(pntLayout) }, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(ShaderStructures::ModoDNMR, ROOT_UNKNOWN, L"ModoDNVS.cso", L"ModoDNMRPS.cso", nullptr, { pntLayout, _countof(pntLayout) }, geometry, State::RenderPass::Geometry));
#endif
	shaderTasks.push_back(CreateShader(ShaderStructures::SSAOShader, ROOT_UNKNOWN, L"FSQuadViewPosVS.cso", L"SSAOPS.cso", nullptr, { {}, 0 }, ssao, State::RenderPass::Lighting));
	completionTask_ = Concurrency::when_all(std::begin(shaderTasks), std::end(shaderTasks)).then([this]() { shaderTasks.clear(); });;
}

Concurrency::task<void> PipelineStates::CreateShader(ShaderId id,
		size_t rootSignatureIndex,
		const wchar_t* vs,
		const wchar_t* ps,
		const wchar_t* gs,
		const D3D12_INPUT_LAYOUT_DESC il,
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
		State::RenderPass pass) {
	auto vsTask = DX::ReadDataAsync(vs).then([](std::vector<byte>& data) {
		return std::make_shared<std::vector<byte>>(std::move(data));
	});
	auto psTask = DX::ReadDataAsync(ps).then([](std::vector<byte>& data) {
		return std::make_shared<std::vector<byte>>(std::move(data));
	});
	concurrency::task<std::shared_ptr<std::vector<byte>>> gsTask;
	task<std::vector<std::shared_ptr<std::vector<byte>>>> allTasks;
	if (gs) {
		gsTask = DX::ReadDataAsync(gs).then([](std::vector<byte>& data) {
			return std::make_shared<std::vector<byte>>(std::move(data));
		});
		allTasks = (vsTask && psTask && gsTask);
	} else allTasks = (vsTask && psTask);

	return allTasks.then([=](const std::vector<std::shared_ptr<std::vector<byte>>>& res) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = desc;
		if (il.NumElements) state.InputLayout = il;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		auto name = (std::wstring{ ps } + std::to_wstring(id));
		if (rootSignatureIndex == ROOT_UNKNOWN) {
			//state.pRootSignature = nullptr;
			ComPtr<ID3DBlob> blob;
			// rs from ps
			::D3DGetBlobPart(res[1]->data(), res[1]->size(), D3D_BLOB_ROOT_SIGNATURE, 0, blob.GetAddressOf());
			DX::ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf())));
			state.pRootSignature = rootSignature.Get();
			// TODO:: seems to be d3d somehow recognizes the same rootsignatures and returns an existing one instead of creating a new one
			rootSignature->SetName(name.c_str());
			// don't store it in rootSignatures_ not thread safe
		} else {
			rootSignature = rootSignatures_[rootSignatureIndex];
			state.pRootSignature = rootSignature.Get();
		}
		state.VS = CD3DX12_SHADER_BYTECODE(&res[0]->front(), res[0]->size());
		state.PS = CD3DX12_SHADER_BYTECODE(&res[1]->front(), res[1]->size());
		if (res.size() > 2) {
			state.GS = CD3DX12_SHADER_BYTECODE(&res[2]->front(), res[2]->size());
		}
		ComPtr<ID3D12PipelineState> pipelineState;
		DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(pipelineState.GetAddressOf())));
		pipelineState->SetName(name.c_str());
		states_[id] = { rootSignature, pipelineState, pass};
	});
}
Concurrency::task<void> PipelineStates::CreateComputeShader(ShaderId id,
	size_t rootSignatureIndex,
	const wchar_t* cs,
	State::RenderPass pass) {
	return DX::ReadDataAsync(cs).then([=](std::vector<byte>& data) {
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = { nullptr,
			CD3DX12_SHADER_BYTECODE(data.data(), data.size()),
			0,
			{},
			D3D12_PIPELINE_STATE_FLAG_NONE
		};
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		auto name = std::wstring{ cs } +std::to_wstring(id);
		if (rootSignatureIndex == ROOT_UNKNOWN) {
			//state.pRootSignature = nullptr;
			ComPtr<ID3DBlob> blob;
			// rs from ps
			::D3DGetBlobPart(data.data(), data.size(), D3D_BLOB_ROOT_SIGNATURE, 0, blob.GetAddressOf());
			DX::ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf())));
			desc.pRootSignature = rootSignature.Get();
			rootSignature->SetName(name.c_str());
			// don't store it in rootSignatures_ not thread safe
		} else {
			rootSignature = rootSignatures_[rootSignatureIndex];
			desc.pRootSignature = rootSignature.Get();
		}
		ComPtr<ID3D12PipelineState> pipelineState;
		DX::ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(pipelineState.GetAddressOf())));
		pipelineState->SetName(name.c_str());
		states_[id] = { rootSignature, pipelineState, pass};
	});
}