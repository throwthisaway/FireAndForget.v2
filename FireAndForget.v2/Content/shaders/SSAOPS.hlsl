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
static const float kEpsilon = .0001f;

ConstantBuffer<SSAOScene> scene : register(b0);
ConstantBuffer<AO> ao : register(b1);
struct Kernel {
	float3 data[kKernelSize];
};
ConstantBuffer<Kernel> kernel: register(b2);

Texture2D<float> tDepth : register(t0);
Texture2D<float3> tNormal : register(t1);
Texture2D<float3> tRandom : register(t2);
SamplerState smpLinearClamp : register(s0);
SamplerState smpLinearWrap : register(s1);

// from: https://github.com/d3dcoder/d3d12book/blob/master/Chapter%2021%20Ambient%20Occlusion/Ssao/Shaders/Ssao.hlsl
float NDCDepthToViewDepth(float zNDC, float4x4 proj) {
	// z_ndc = A + B/viewZ, where proj[2,2]=A and proj[3,2]=B.
	// [3][2] => [2][3] due scene.proj is row_major
	// [3][2] is not adressing [3].z instead into the kernel array for some reason
	float p23 = proj[3].z, p22 = proj[2].z;
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
struct MRTOut {
	float ao : SV_TARGET0;
	float4 debug : SV_TARGET1;
};
[RootSignature(RS)]
MRTOut main(PS_PUV input) {
	MRTOut result;
	float d = tDepth.Sample(smpLinearClamp, input.uv).x;
	float3 n = normalize(tNormal.Sample(smpLinearClamp, input.uv).xyz);	// f16 no need to transform
	n = mul(n, scene.view);
	float3 random = tRandom.Sample(smpLinearWrap, input.uv * ao.randomFactor).rgb * 2.f - 1.f;
	float3 p = ViewPosFromDepth(d, input.p/*near clip plane viewpos*/);
	float occlusion = 0;
	// https://github.com/d3dcoder/d3d12book/blob/master/Chapter%2021%20Ambient%20Occlusion/Ssao/Shaders/Ssao.hlsl
	for (int i = 0; i < kKernelSize; ++i) {
		float3 sample = kernel.data[i];
		//sample = reflect(random, sample);
		float flip = sign(dot(sample, n));
		float3 offset = flip * sample * ao.rad;
		float3 q = p + offset * ao.scale; // q in view  space
		float4 qNDC = mul(float4(q, 1.f), scene.proj);
		qNDC /= qNDC.w;
		qNDC.xy = qNDC.xy * float2(.5f, -.5f) + .5f;
		
		float rz = tDepth.Sample(smpLinearClamp, qNDC.xy).x;
		rz = NDCDepthToViewDepth(rz, scene.proj);
		// r = t*q, rz = t*qz => r = rz/qz * q;
		float3 r = (rz / q.z) * q;

		// occlusion factor
		float occlusionFactor =	0.f;
		float distZ = r.z - p.z;
		if (distZ > kEpsilon) {
			float fadeLength = ao.fadeEnd - ao.fadeStart;
			occlusionFactor = saturate((ao.fadeEnd - distZ) / fadeLength);
		}
		float3 rp = normalize(r - p);
		occlusion += occlusionFactor * ao.intensity * max(dot(rp, n), 0.f);

	}
	occlusion /= kKernelSize;
	result.ao = 1.f - occlusion;
	result.ao = pow(result.ao, ao.bias);	
	result.debug = float4(p, 1.f);
	return result;
}