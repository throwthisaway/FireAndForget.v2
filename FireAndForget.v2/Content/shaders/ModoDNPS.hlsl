#include "Common.hlsli"
#include "ShaderInput.hlsli"
#include "ShaderStructs.hlsli"
#include "ModoDNRS.hlsli"

ConstantBuffer<Material> mat : register(b0);

Texture2D<float4> tDiffuse : register(t0);
Texture2D<float4> tNormal : register(t1);

SamplerState smp : register(s0);

[RootSignature(ModoDNRS)]
MRTOut main(PS_UVNT input) {
	MRTOut output;
	output.albedo = tDiffuse.Sample(smp, input.uv);
	//	float3 nTex = tNormal.Sample(smp, input.uv).rgb * 255.f/127.f - 128.f/127.f;
	float3 nTex = tNormal.Sample(smp, input.uv).rgb * 2.f - 1.f;
	float3 nWS = normalize(input.nWS);
	float3 t = normalize(input.t - dot(input.t, nWS)*nWS);
	float3 b = cross(nWS, t);
	float3x3 tbn = float3x3(t, b, nWS);
	output.normalVS = float4(normalize(input.nVS), 1.f);
	nWS = mul(nTex, tbn);
	output.normalWS = float4(nWS, 1.f);

	output.material = float4(mat.metallic_roughness, 0.f, 1.f);
	//output.debug = float4(n, 1.f);
	return output;
}