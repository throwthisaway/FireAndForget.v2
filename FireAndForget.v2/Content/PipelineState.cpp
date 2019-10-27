#include "pch.h"
#include "d3dcompiler.h"
#include "PipelineState.h"
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"
#include <string>
using namespace Microsoft::WRL;
using namespace concurrency;
using namespace ShaderStructures;

namespace {
	/*const D3D12_INPUT_ELEMENT_DESC debugInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };*/
	const D3D12_INPUT_ELEMENT_DESC pnLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };
	const D3D12_INPUT_ELEMENT_DESC pnuvLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };
	const D3D12_INPUT_ELEMENT_DESC pntuvLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };
	const D3D12_INPUT_ELEMENT_DESC puvLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };
	/*const D3D12_INPUT_ELEMENT_DESC fsQuadLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };*/

	struct VSDesc {
		D3D12_INPUT_LAYOUT_DESC il;
		const wchar_t* vs;
	} pos = { { pnLayout, _countof(pnLayout) }, L"PosVS.cso" },
		tex = { { pnuvLayout, _countof(pnuvLayout) }, L"TexVS.cso" },
		debug = { { pnLayout, _countof(pnLayout) }, L"DebugVS.cso" },
		fsQuad = { { {}, 0 }, L"FSQuadVS.cso" },
		cubeEnvMap = { {pnLayout, _countof(pnLayout) }, L"CubeEnvMapVS.cso" },
		bg = { {pnLayout, _countof(pnLayout) },  L"BgVS.cso" },
		modoDN = { { pntuvLayout, _countof(pntuvLayout) }, L"ModoDNVS.cso" },
		fsQuadViewPos = { { {}, 0 }, L"FSQuadViewPosVS.cso" },
		shadowPos = { { pnLayout, _countof(pnLayout) }, L"ShadowPosVS.cso" },
		shadowModoDN = { { pntuvLayout, _countof(pntuvLayout) }, L"ShadowModoDNVS.cso" };



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
			downsampleDepth = state;
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
			r32 = state;
		}

		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
			state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			state.RasterizerState.DepthBias = 1000000;
			state.RasterizerState.SlopeScaledDepthBias = 1.f;
			state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			state.SampleMask = UINT_MAX;
			state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			state.NumRenderTargets = 0;
			state.RTVFormats[0] = DXGI_FORMAT_UNKNOWN; 
			state.DSVFormat = depthbufferFormat;
			state.SampleDesc.Count = 1;
			shadow = state;
		}
}

PipelineStates::~PipelineStates() {}

void PipelineStates::CreateDeviceDependentResources() {

	rootSignatures_.resize(ROOT_SIG_COUNT);

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
	{
		// ROOT_VS_1CB
		CD3DX12_ROOT_PARAMETER parameter[1];
		CD3DX12_DESCRIPTOR_RANGE range0;
		range0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		parameter[0].InitAsDescriptorTable(1, &range0, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(_ARRAYSIZE(parameter), parameter, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> pSignature;
		ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		ComPtr<ID3D12RootSignature>	rootSignature;
		DX::ThrowIfFailed(device->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
		NAME_D3D12_OBJECT(rootSignature);
		rootSignatures_[ROOT_VS_1CB] = rootSignature;
	}
	shaderTasks.reserve(Count);
	shaderTasks.push_back(CreateShader(Pos, ROOT_VS_1CB_PS_1CB, pos.il, pos.vs, L"PosPS.cso", nullptr, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(Tex, ROOT_VS_1CB_PS_1TX_1CB, tex.il, tex.vs, L"TexPS.cso", nullptr, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(Debug, ROOT_VS_1CB_PS_1CB, debug.il, debug.vs, L"DebugPS.cso", nullptr, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(Deferred, ROOT_UNKNOWN, fsQuad.il, fsQuad.vs, L"DeferredPS.cso", nullptr,  lighting, State::RenderPass::Lighting));
	shaderTasks.push_back(CreateShader(DeferredPBR, ROOT_UNKNOWN, fsQuad.il, fsQuad.vs, L"DeferredPBRPS.cso", nullptr, lighting, State::RenderPass::Lighting));

	shaderTasks.push_back(CreateShader(CubeEnvMap, ROOT_UNKNOWN, cubeEnvMap.il, cubeEnvMap.vs, L"CubeEnvMapPS.cso", nullptr, pre, State::RenderPass::Pre));
	shaderTasks.push_back(CreateShader(Bg, ROOT_UNKNOWN, bg.il, bg.vs, L"BgPS.cso", nullptr, forward, State::RenderPass::Forward));
	shaderTasks.push_back(CreateShader(Irradiance, ROOT_UNKNOWN, cubeEnvMap.il, cubeEnvMap.vs, L"CubeEnvIrPS.cso", nullptr, pre, State::RenderPass::Pre));
	shaderTasks.push_back(CreateShader(PrefilterEnv, ROOT_UNKNOWN, cubeEnvMap.il, cubeEnvMap.vs, L"CubeEnvPrefilterPS.cso", nullptr, pre, State::RenderPass::Pre));
	shaderTasks.push_back(CreateShader(BRDFLUT, ROOT_UNKNOWN, fsQuad.il, fsQuad.vs, L"BRDFLUTPS.cso", nullptr, rg16, State::RenderPass::Pre));
	shaderTasks.push_back(CreateShader(Downsample, ROOT_UNKNOWN, fsQuad.il, fsQuad.vs, L"DownsampleDepthPS.cso", nullptr, downsampleDepth, State::RenderPass::Lighting));
	shaderTasks.push_back(CreateComputeShader(GenMips, ROOT_UNKNOWN, L"GenMips.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(GenMipsOddX, ROOT_UNKNOWN, L"GenMipsOddX.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(GenMipsOddY, ROOT_UNKNOWN, L"GenMipsOddY.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(GenMipsOddXOddY, ROOT_UNKNOWN, L"GenMipsOddXOddY.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(GenMipsSRGB, ROOT_UNKNOWN, L"GenMipsSRGB.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(GenMipsOddXSRGB, ROOT_UNKNOWN, L"GenMipsOddXSRGB.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(GenMipsOddYSRGB, ROOT_UNKNOWN, L"GenMipsOddYSRGB.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateComputeShader(GenMipsOddXOddYSRGB, ROOT_UNKNOWN, L"GenMipsOddXOddYSRGB.cso", State::RenderPass::Pre));
	shaderTasks.push_back(CreateShader(ModoDN, ROOT_UNKNOWN, modoDN.il, modoDN.vs, L"ModoDNPS.cso", nullptr, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(ModoDNMR, ROOT_UNKNOWN, modoDN.il, modoDN.vs, L"ModoDNMRPS.cso", nullptr, geometry, State::RenderPass::Geometry));
	shaderTasks.push_back(CreateShader(SSAOShader, ROOT_UNKNOWN, fsQuadViewPos.il, fsQuadViewPos.vs, L"SSAOPS.cso", nullptr, ssao, State::RenderPass::Lighting));
	shaderTasks.push_back(CreateShader(Blur4x4R32, ROOT_UNKNOWN, fsQuad.il, fsQuad.vs, L"Blur4x4R32.cso", nullptr, r32, State::RenderPass::Post));
	
	shaderTasks.push_back(CreateShader(ShadowPos, ROOT_VS_1CB, shadowPos.il, shadowPos.vs, L"ShadowPS.cso", nullptr, shadow, State::RenderPass::Shadow));
	shaderTasks.push_back(CreateShader(ShadowModoDN, ROOT_VS_1CB, shadowModoDN.il, shadowModoDN.vs, L"ShadowPS.cso", nullptr, shadow, State::RenderPass::Shadow));

	shaderTasks.push_back(CreateShader(ModoDNB, ROOT_UNKNOWN, modoDN.il, modoDN.vs, L"ModoDNBPS.cso", nullptr, geometry, State::RenderPass::Geometry));
	
	completionTask = Concurrency::when_all(std::begin(shaderTasks), std::end(shaderTasks)).then([this]() { shaderTasks.clear(); });;
}

PipelineStates::CreateShaderTask PipelineStates::CreateShadowShader(ShaderId id,
	size_t rootSignatureIndex,
	const D3D12_INPUT_LAYOUT_DESC il,
	const wchar_t* vs,
	const wchar_t* gs,
	D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
	State::RenderPass pass) {
	std::vector<task<std::shared_ptr<std::vector<byte>>>> allTasks;

	auto vsTask = DX::ReadDataAsync(vs).then([](std::vector<byte>& data) {
		return std::make_shared<std::vector<byte>>(std::move(data));
	});
	allTasks.push_back(vsTask);
	if (gs) {
		auto gsTask = DX::ReadDataAsync(gs).then([](std::vector<byte>& data) {
			return std::make_shared<std::vector<byte>>(std::move(data));
			});
		allTasks.push_back(gsTask);
	}
	return Concurrency::when_all(std::begin(allTasks), std::end(allTasks)).then([=](const std::vector<std::shared_ptr<std::vector<byte>>>& res) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = desc;
		if (il.NumElements) state.InputLayout = il;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		auto name = (std::wstring{ vs } + std::to_wstring(id));
		rootSignature = rootSignatures_[rootSignatureIndex];
		state.pRootSignature = rootSignature.Get();
		state.VS = CD3DX12_SHADER_BYTECODE(&res[0]->front(), res[0]->size());
		if (res.size() > 1) {
			state.GS = CD3DX12_SHADER_BYTECODE(&res[1]->front(), res[1]->size());
		}
		ComPtr<ID3D12PipelineState> pipelineState;
		DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(pipelineState.GetAddressOf())));
		pipelineState->SetName(name.c_str());
		states[id] = { rootSignature, pipelineState, pass};
	});
}
PipelineStates::CreateShaderTask PipelineStates::CreateShader(ShaderId id,
		size_t rootSignatureIndex,
		const D3D12_INPUT_LAYOUT_DESC il,
		const wchar_t* vs,
		const wchar_t* ps,
		const wchar_t* gs,
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
		State::RenderPass pass) {
	std::vector<task<std::shared_ptr<std::vector<byte>>>> allTasks;

	auto vsTask = DX::ReadDataAsync(vs).then([](std::vector<byte>& data) {
		return std::make_shared<std::vector<byte>>(std::move(data));
	});
	allTasks.push_back(vsTask);
	auto psTask = DX::ReadDataAsync(ps).then([](std::vector<byte>& data) {
		return std::make_shared<std::vector<byte>>(std::move(data));
		});
	allTasks.push_back(psTask);
	if (gs) {
		auto gsTask = DX::ReadDataAsync(gs).then([](std::vector<byte>& data) {
			return std::make_shared<std::vector<byte>>(std::move(data));
			});
		allTasks.push_back(gsTask);
	}

	return Concurrency::when_all(std::begin(allTasks), std::end(allTasks)).then([=](const std::vector<std::shared_ptr<std::vector<byte>>>& res) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = desc;
		if (il.NumElements) state.InputLayout = il;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		auto name = (std::wstring{ ps } + std::to_wstring(id));
		if (rootSignatureIndex == ROOT_UNKNOWN) {
			assert(res.size() > 1);
			//state.pRootSignature = nullptr;
			ComPtr<ID3DBlob> blob;
			// rs from ps
			::D3DGetBlobPart(res[1]->data(), res[1]->size(), D3D_BLOB_ROOT_SIGNATURE, 0, blob.GetAddressOf());
			DX::ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf())));
			state.pRootSignature = rootSignature.Get();
			// seems to be d3d somehow recognizes the same rootsignatures and returns an existing one instead of creating a new one
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
		states[id] = { rootSignature, pipelineState, pass};
	});
}
PipelineStates::CreateShaderTask PipelineStates::CreateComputeShader(ShaderId id,
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
		states[id] = { rootSignature, pipelineState, pass};
	});
}