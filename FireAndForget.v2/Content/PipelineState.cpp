#include "pch.h"
#include "PipelineState.h"
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"

using namespace Microsoft::WRL;

PipelineStates::PipelineStates(const DX::DeviceResources* deviceResources) :
	deviceResources_(deviceResources) {}


PipelineStates::~PipelineStates() {}

void PipelineStates::CreateDeviceDependentResources() {

	auto d3dDevice = deviceResources_->GetD3DDevice();

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

	std::shared_ptr<std::vector<byte>> vertexShader = std::make_shared<std::vector<byte>>(),
		pixelShader = std::make_shared<std::vector<byte>>();
	
	auto createVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso").then([this, vertexShader](std::vector<byte>& fileData) mutable {
		*vertexShader = std::move(fileData);
	});
	auto createPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso").then([this, pixelShader](std::vector<byte>& fileData) mutable {
		*pixelShader = std::move(fileData);
	});

	ComPtr<ID3D12PipelineState> pipelineState;
	auto createPipelineStateTask = (createPSTask && createVSTask).then([this, rootSignature, pipelineState, vertexShader, pixelShader]() mutable {

		static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
		state.InputLayout = { inputLayout, _countof(inputLayout) };
		state.pRootSignature = rootSignature.Get();
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


		rootSignatures_.push_back(rootSignature);
		states_.push_back({ rootSignatures_.size()-1, pipelineState });
	});
	completionTask_ = createPipelineStateTask.then([this]() {
		loadingComplete_ = true;
	});
}
