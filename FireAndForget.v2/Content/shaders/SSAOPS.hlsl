#include "ShaderInput.hlsli"
#include "ShaderStructs.h"
#define RS "RootFlags(DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
		"DescriptorTable(CBV(b0), visibility = SHADER_VISIBILITY_VERTEX)," \
		"DescriptorTable(CBV(b0, numDescriptors = 3)," \
						"SRV(t0, numDescriptors = 3), visibility = SHADER_VISIBILITY_PIXEL)," \
		"StaticSampler(s0," \
			"addressU = TEXTURE_ADDRESS_CLAMP," \
			"addressV = TEXTURE_ADDRESS_CLAMP," \
			"addressW = TEXTURE_ADDRESS_CLAMP," \
			"filter = FILTER_MIN_MAG_MIP_LINEAR," \
			"visibility = SHADER_VISIBILITY_PIXEL),"\
		"StaticSampler(s1," \
			"addressU = TEXTURE_ADDRESS_WRAP," \
			"addressV = TEXTURE_ADDRESS_WRAP," \
			"addressW = TEXTURE_ADDRESS_WRAP," \
			"filter = FILTER_MIN_MAG_MIP_LINEAR," \
			"visibility = SHADER_VISIBILITY_PIXEL)"
static const int kKernelSize = 14;
ConstantBuffer<SSAOScene> scene : register(b0);
ConstantBuffer<AO> ao : register(b1);
cbuffer cb : register(b2) {
	float3 kernel[kKernelSize];
};

Texture2D<float> tDepth : register(t0);
Texture2D<float3> tNormal : register(t1);
Texture2D<float3> tRandom : register(t2);
SamplerState smpLinearClamp : register(s0);
SamplerState smpLinearWrap : register(s1);

// from: https://github.com/d3dcoder/d3d12book/blob/master/Chapter%2021%20Ambient%20Occlusion/Ssao/Shaders/Ssao.hlsl
float NDCDepthToViewDepth(float zNDC, float4x4 proj) {
	// z_ndc = A + B/viewZ, where proj[2,2]=A and proj[3,2]=B.
	// [3][2] => [2][3] due scene.proj is row_major
	float p23 = proj[3][2], p22 = proj[2][2];
	return p23 / (zNDC - p22);
}
float3 ViewPosFromDepth(float depth, float3 pos) {
	float z = NDCDepthToViewDepth(depth, scene.proj);
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pos.
	// p.z = t*pos.z
	// t = p.z / pos.z
	return (z / pos.z) * pos;
}
//float3 ViewPosFormDepth2(float2 uv, float4x4 ip, float depth) {
//	float4 projected_pos = float4(uv * 2.f - 1.f, depth, 1.f);
//	projected_pos.y = -projected_pos.y;
//	float4 viewPos = mul(ip, projected_pos);
//	return viewPos.xyz / viewPos.w;
//}
//float AOPass(float2 uv, float3 center, float3 n) {
//	float d = depth.Sample(smp, uv).x;
//	float3 viewPos = ViewPosFormDepth(d, center);
//	float3 diff = viewPos - center;
//	float len = length(diff);
//	float3 v = diff / len;
//	len *= ao.scale;
//	return max(0.f, dot(v, n) - ao.bias) * 1.f / (1.f + len) * ao.intensity;
//}
//
//#define ITERATION 4
//float CalcAO(float2 uv, float3 center_pos, float3 n) {
//	const float2 sampling[] = { float2(-1.f, 0.f), float2(1.f, 0.f), float2(0.f, 1.f), float2(0.f, -1.f) };
//
//	float sum = 0.f, radius = ao.rad / center_pos.z;
//	float2 rnd = normalize(random.Sample(smp, scene.viewport * uv / ao.random_size).rg * 2.f - 1.f);
//	// TODO:: int iterations = lerp(6.0,2.0,p.z/g_far_clip);
//	for (int i = 0; i < ITERATION; i++) {
//		float2 c1 = reflect(sampling[i], rnd) * radius
//		, c2 = float2(c1.x * .70716f - c1.y * .70716f,
//					  c1.x * .70716f + c1.y * .70716f);
//
//		sum += AOPass(uv + c1 * .25f, center_pos, n);
//		sum += AOPass(uv + c1 * .75f, center_pos, n);
//		sum += AOPass(uv + c2 * .5f, center_pos, n);
//		sum += AOPass(uv + c2, center_pos, n);
//	}
//	sum /= (float)ITERATION * 4.f;
//	return sum;
//}
struct MRTOut {
	float ao : SV_TARGET0;
	float4 debug : SV_TARGET1;
};
[RootSignature(RS)]
MRTOut main(PS_PUV input) {
	//float d = tDepth.Sample(smp, input.uv).x;
	//float3 viewPos = ViewPosFromDepth(d, input.p);
	////float3 worldPos = ViewPosFormDepth2(input.uv, scene.ip, d);
	//float3 n = normal.Sample(smp, input.uv);
	//return CalcAO(input.uv, viewPos, n);
	float d = tDepth.Sample(smpLinearClamp, input.uv).x;
	float3 n = tNormal.Sample(smpLinearClamp, input.uv).xyz;	// f16 no need to transform
	float3 random = tRandom.Sample(smpLinearWrap, input.uv * ao.randomFactor).rgb * 2.f - 1.f;
	float3 p = ViewPosFromDepth(d, input.p);
	float occlusion = 0;
	for (int i = 0; i < kKernelSize; ++i) {
		float3 sample = kernel[i];
		sample = reflect(random, sample);
		float flip = sign(dot(sample, n));
		float3 offset = flip * sample * ao.rad;
		float3 q = p + offset *ao.scale; // q in view space
		float4 qNDC = mul(float4(q, 1.f), scene.proj);
		qNDC.xy = qNDC.xy * .5f + .5f;
		qNDC.z /= qNDC.w;
		float rd = tDepth.Sample(smpLinearClamp, qNDC.xy).x;
		float3 r = ViewPosFromDepth(rd, qNDC.xyz);
		float3 rp = normalize(r - p);
		occlusion += ao.intensity* dot(rp, n);
		// r = t*q, rz = t*qz => r = rz/qz * q;
		// TODO:: fade out
	}
	occlusion /= kKernelSize;
	MRTOut result;
	result.ao = 1.f - occlusion;
	result.debug = float4(random * .5f +.5f, 1.f);// float4(p, 1.f);
	return result;
}