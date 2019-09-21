#include "Common.hlsli"
#include "ShaderInput.hlsli"
#include "ShaderStructs.h"

Texture2D<float4> tColor : register(t0);
ConstantBuffer<Material> mat : register(b0);

SamplerState smp : register(s0) {
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

MRTOut main(PS_UVN input) {
	float4 diffuseColor = tColor.Sample(smp, input.uv);
	MRTOut output;
	output.albedo = diffuseColor;
	float3 nWS = normalize(input.nWS);
	output.normalWS = float4(nWS, 1.f);
	output.normalVS = float4(normalize(input.nVS), 1.f);
	output.material = float4(mat.metallic_roughness, 0.f, 1.f);
	output.debug = float4(nWS, 1.f);
	return output;
}