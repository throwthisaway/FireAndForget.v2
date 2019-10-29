#include "Common.hlsli"
#include "ShaderInput.hlsli"
#include "ShaderStructs.hlsli"

#define ModoDNBRS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
			"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
    "DescriptorTable(CBV(b0, numDescriptors = 2), visibility = SHADER_VISIBILITY_VERTEX)," \
    "DescriptorTable(SRV(t0, numDescriptors = 3), CBV(b0),visibility = SHADER_VISIBILITY_PIXEL)," \
    "StaticSampler(s0, visibility = SHADER_VISIBILITY_PIXEL," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

ConstantBuffer<Material> mat : register(b0);

Texture2D<float4> tDiffuse : register(t0);
Texture2D<float4> tNormal : register(t1);
Texture2D<float4> tBump : register(t2);

SamplerState smp : register(s0);

float2 Parallax(float2 uv, float3 viewDirTS) {
	float d = tBump.Sample(smp, uv).x;
	float offset = viewDirTS.xy / viewDirTS.z * d * .02;
	return uv - offset;
}
[RootSignature(ModoDNBRS)]
MRTOut main(PS_PUVNTVP input) {
	MRTOut output;

	float3 viewDirTS = input.vTS - input.pTS;
	float2 uv = Parallax(input.uv, viewDirTS);
	output.albedo = tDiffuse.Sample(smp, uv);
	//	float3 nTex = tNormal.Sample(smp, input.uv).rgb * 255.f/127.f - 128.f/127.f;
	float3 nTex = tNormal.Sample(smp, uv).rgb * 2.f - 1.f;
	float3 nWS = normalize(input.nWS);
	float3 t = normalize(input.tWS - dot(input.tWS, nWS) * nWS);
	float3 b = cross(nWS, t);
	float3x3 tbn = float3x3(t, b, nWS);
	output.normalVS = float4(normalize(input.nVS), 1.f);
	nWS = normalize(mul(nTex, tbn));
	output.normalWS = float4(nWS, 1.f);

	output.material = float4(mat.metallic_roughness, 0.f, 1.f);
	output.positionWS = float4(input.pWS, 1.f);
	// TODO::
	return output;
}