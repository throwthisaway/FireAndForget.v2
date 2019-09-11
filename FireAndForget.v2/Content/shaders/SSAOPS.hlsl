#include "ShaderInput.hlsli"
#include "ShaderStructs.h"
#define RS "RootFlags(DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
		"DescriptorTable(CBV(b0), visibility = SHADER_VISIBILITY_VERTEX)," \
		"DescriptorTable(CBV(b0, numDescriptors = 2)," \
						"SRV(t0, numDescriptors = 3), visibility = SHADER_VISIBILITY_PIXEL)," \
		"StaticSampler(s0," \
			"addressU = TEXTURE_ADDRESS_CLAMP," \
			"addressV = TEXTURE_ADDRESS_CLAMP," \
			"addressW = TEXTURE_ADDRESS_CLAMP," \
			"filter = FILTER_MIN_MAG_MIP_LINEAR," \
			"visibility = SHADER_VISIBILITY_PIXEL)"
ConstantBuffer<SSAOScene> scene : register(b0);
ConstantBuffer<AO> ao : register(b1);
Texture2D<float> depth : register(t0);
Texture2D<float3> normal : register(t1);
Texture2D<float2> random : register(t2);
SamplerState smp : register(s0);

// from: https://github.com/d3dcoder/d3d12book/blob/master/Chapter%2021%20Ambient%20Occlusion/Ssao/Shaders/Ssao.hlsl
float NDCDepthToViewDepth(float zNDC, float4x4 proj) {
	// z_ndc = A + B/viewZ, where proj[2,2]=A and proj[3,2]=B.
	// [3][2] => [2][3] due to glm::mat4 in-memory representation
	// seems like column major
	float p23 = proj[2][3], p22 = proj[2][2];
	return p23 / (zNDC - p22);
}
float3 WorldPosFormDepth(float depth, float3 pos) {
	float z = NDCDepthToViewDepth(depth, scene.proj);
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pos.
	// p.z = t*pos.z
	// t = p.z / pos.z
	return (z / pos.z) * pos;
}
float3 WorldPosFormDepth2(float2 uv, float4x4 ip, float depth) {
	float4 projected_pos = float4(uv * 2.f - 1.f, depth, 1.f);
	projected_pos.y = -projected_pos.y;
	float4 world_pos = mul(ip, projected_pos);
	return world_pos.xyz / world_pos.w;
}
float AOPass(float2 uv, float3 center, float3 n) {
	float d = depth.Sample(smp, uv).x;
	float3 worldPos = WorldPosFormDepth(d, center);
	float3 diff = worldPos - center;
	float len = length(diff);
	float3 v = diff / len;
	len *= ao.scale;
	return max(0.f, dot(v, n) - ao.bias) * 1.f / (1.f + len) * ao.intensity;
}

#define ITERATION 4
float CalcAO(float2 uv, float3 center_pos, float3 n) {
	const float2 sampling[] = { float2(-1.f, 0.f), float2(1.f, 0.f), float2(0.f, 1.f), float2(0.f, -1.f) };

	float sum = 0.f, radius = ao.rad / center_pos.z;
	float2 rnd = normalize(random.Sample(smp, scene.viewport * uv / ao.random_size).rg * 2.f - 1.f);
	// TODO:: int iterations = lerp(6.0,2.0,p.z/g_far_clip);
	for (int i = 0; i < ITERATION; i++) {
		float2 c1 = reflect(sampling[i], rnd) * radius
		, c2 = float2(c1.x * .70716f - c1.y * .70716f,
					  c1.x * .70716f + c1.y * .70716f);

		sum += AOPass(uv + c1 * .25f, center_pos, n);
		sum += AOPass(uv + c1 * .75f, center_pos, n);
		sum += AOPass(uv + c2 * .5f, center_pos, n);
		sum += AOPass(uv + c2, center_pos, n);
	}
	sum /= (float)ITERATION * 4.f;
	return sum;
}

[RootSignature(RS)]
float main(PS_PUV input) : SV_TARGET{
	float d = depth.Sample(smp, input.uv).x;
	float3 worldPos2 = WorldPosFormDepth(d, input.p);
	float3 worldPos = WorldPosFormDepth2(input.uv, scene.ip, d);
	float3 n = normal.Sample(smp, input.uv);
	return CalcAO(input.uv, worldPos, n);
}