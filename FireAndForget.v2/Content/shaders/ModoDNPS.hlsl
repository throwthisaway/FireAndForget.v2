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
MRTOut main(PS_UVNT input) {
	MRTOut output;
	output.albedo = tDiffuse.Sample(smp, input.uv);
	float3 nTex = tNormal.Sample(smp, input.uv).rgb * 2.f - 1.f;
	float3 n = normalize(input.n);
	float3 t = input.t;
	t = normalize(t - dot(n, t)*n);
	float3 b = cross(t, n);
	float3x3 tbn = float3x3(t, b, n);
	n = mul(tbn, nTex);
	output.normal = Encode(n);
	output.material = float4(mat.metallic_roughness, 0.f, 1.f);
	output.debug = float4(n, 1.f);
	return output;
}