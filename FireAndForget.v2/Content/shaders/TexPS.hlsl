#include "Common.hlsli"
#include "PSInput.hlsli"
#include "ShaderStructs.h"

Texture2D<float4> tColor : register(t0);
ConstantBuffer<Material> mat : register(b0);

SamplerState smp : register(s0) {
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

MRTOut main(PS_TN input) {
	float4 diffuseColor = tColor.Sample(smp, input.uv);
	MRTOut output;
	output.albedo = diffuseColor;
	output.normal = Encode(input.n);
	output.material = float4(mat.metallic_roughness, 0.f, 1.f);
	output.debug = float4(input.n, 1.f);
	return output;
}