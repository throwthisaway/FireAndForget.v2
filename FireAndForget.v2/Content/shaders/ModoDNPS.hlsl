#include "Common.hlsli"
#include "PSInput.hlsli"
#include "ShaderStructs.h"
#include "ModoDNRS.hlsli"

ConstantBuffer<Material> mat : register(b0);

Texture2D<float4> tDiffuse : register(t0);
Texture2D<float4> tNormal : register(t1);

SamplerState smp : register(s0) {
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

[RootSignature(ModoDNRS)]
MRTOut main(PS_TN_TBN input) {
	MRTOut output;
	output.albedo = tDiffuse.Sample(smp, input.uv);
	float3 n = normalize(tNormal.Sample(smp, input.uv).rgb * 2.f - 1.f);
	//output.normal = Encode(normalize(mul(input.tbn, n)));
	n = normalize(mul(input.tbn, n));
	output.normal = Encode(n);
	output.material = float4(mat.metallic_roughness, 0.f, 1.f);
	output.debug = float4(n, 1.f);
	return output;
}