#include "ShaderInput.hlsli"
#define Depth2RelaxedConeRS \
	"RootFlags(DENY_VERTEX_SHADER_ROOT_ACCESS |" \
			"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
	"DescriptorTable(CBV(b0)," \
					"SRV(t0)," \
					"visibility = SHADER_VISIBILITY_PIXEL),"\
	"StaticSampler(s0, visibility = SHADER_VISIBILITY_PIXEL," \
		"addressU = TEXTURE_ADDRESS_WRAP," \
		"addressV = TEXTURE_ADDRESS_WRAP," \
		"addressW = TEXTURE_ADDRESS_WRAP," \
		"filter = FILTER_MIN_MAG_MIP_LINEAR)"

cbuffer cb : register(b0) {
	float2 res;
};
Texture2D<float4> relief : register(t0);
SamplerState smp : register(s0);

float DepthToRelaxedCone(float2 uv, float2 offset, float bestRatio) {
	const int stepCount = 128;
	float coneRatio = 1.f;
	float3 src = float3(uv, 0.f);
	float3 dst = float3(uv + offset, 0.f);
	dst.z = relief.Sample(smp, dst.xy).x;
	float3 vec = dst - src;
	vec /= vec.z;

	vec /= 1.f - dst.z; // length of vec inside relief
	float3 step = vec / stepCount;
	float3 rayPos = dst + step;
	for (int i = 0; i < stepCount; ++i) {
		float d = relief.Sample(smp, rayPos.xy).x;
		if (d <= rayPos.z) {
			rayPos += step;
		}
	}

	float srcDepth = relief.Sample(smp, uv);
	if (srcDepth > rayPos.z) coneRatio = length(rayPos.xy - uv) / (srcDepth - rayPos.z);
	if (bestRatio < coneRatio) coneRatio = bestRatio;
	return coneRatio;
}

[RootSignature(Depth2RelaxedConeRS)]
float2 main(PS_UV input) : SV_TARGET {
	int w, h;
	relief.GetDimensions(w, h);

	float d = relief.Load(uint3(input.uv * res, 0)).x;
	float ratio = 1.f;
	// TODO:: this should be invoked multiple times instead of one pass
	for (int x = 0; x < w; ++x)
		for (int y = 0; y < h; ++y) {
			ratio = DepthToRelaxedCone(input.uv, float2(x, y) * float2(1.f/w, 1.f/h), ratio);
		}
	return float2(d, ratio);
}