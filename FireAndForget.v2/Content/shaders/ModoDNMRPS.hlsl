#include "Common.hlsli"
#include "ShaderInput.hlsli"
#include "ShaderStructs.h"
#include "ModoDNMRRS.hlsli"

Texture2D<float4> tDiffuse : register(t0);
Texture2D<float4> tNormal : register(t1);
Texture2D<float4> tMetallic : register(t2);
Texture2D<float4> tRoughness : register(t3);

SamplerState smp : register(s0);

[RootSignature(ModoDNMRRS)]
MRTOut main(PS_UVNT input) {
	MRTOut output;
	output.albedo = tDiffuse.Sample(smp, input.uv);
	//	float3 nTex = tNormal.Sample(smp, input.uv).rgb * 255.f/127.f - 128.f/127.f;
	float3 nTex = tNormal.Sample(smp, input.uv).rgb * 2.f - 1.f;
	float3 n = normalize(input.n);
	float3 t = normalize(input.t - dot(input.t, n)*n);
	float3 b = cross(n, t);
	float3x3 tbn = float3x3(t, b, n);
	n = mul(nTex, tbn);
	output.normal = Encode(n);

	float metallic = tMetallic.Sample(smp, input.uv).r;
	float roughness = tRoughness.Sample(smp, input.uv).r;
	output.material = float4(metallic, roughness, 0.f, 1.f);
	output.debug = float4(n, 1.f);
	return output;
}