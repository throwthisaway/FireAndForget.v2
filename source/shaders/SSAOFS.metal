#include <metal_stdlib>
#include "FragInput.h.metal"
#include "ShaderStructs.h.metal"
#include "ViewPosFromDepth.h.metal"
using namespace metal;

constant int kKernelSize = 14;
constant float kEpsilon = .0001f;

struct Kernel {
	float3 data[kKernelSize];
};

fragment float ssao_fs_main(FS_PUV input[[stage_in]],
				   constant SSAOScene& scene [[buffer(0)]],
				   constant AO& ao [[buffer(1)]],
				   constant Kernel& kern [[buffer(2)]],
				   texture2d<float> tDepth [[texture(0)]],
				   texture2d<float> tNormal [[texture(1)]],
				   texture2d<float> tRandom [[texture(2)]],
				   sampler smpLinearClamp [[sampler(0)]],
				   sampler smpLinearWrap [[sampler(1)]]) {
	float d = tDepth.sample(smpLinearClamp, input.uv).x;
	float3 n = normalize(tNormal.sample(smpLinearClamp, input.uv).xyz);	// f16 no need to transform
	float3 random = tRandom.sample(smpLinearWrap, input.uv * ao.randomFactor).rgb * 2.f - 1.f;
	float3 p = ViewPosFromDepth(d, input.p/*near clip plane viewpos*/, scene.proj);
	float occlusion = 0;
	// https://github.com/d3dcoder/d3d12book/blob/master/Chapter%2021%20Ambient%20Occlusion/Ssao/Shaders/Ssao.hlsl
	for (int i = 0; i < kKernelSize; ++i) {
		float3 sample = kern.data[i];
		sample = reflect(random, sample);
		float flip = sign(dot(sample, n));
		float3 offset = flip * sample * ao.rad;
		float3 q = p + offset * ao.scale; // q in view  space
		float4 qNDC = scene.proj * float4(q, 1.f);
		qNDC /= qNDC.w;
		qNDC.xy = qNDC.xy * float2(.5f, -.5f) + .5f;

		float rz = tDepth.sample(smpLinearClamp, qNDC.xy).x;
		rz = NDCDepthToViewDepth(rz, scene.proj);
		// r = t*q, rz = t*qz => r = rz/qz * q;
		float3 r = (rz / q.z) * q;

		// occlusion factor
		float occlusionFactor =	0.f;
		float distZ = abs(r.z - p.z);
		if (distZ > kEpsilon) {
			float fadeLength = ao.fadeEnd - ao.fadeStart;
			occlusionFactor = saturate((ao.fadeEnd - distZ) / fadeLength);
		}
		float3 rp = normalize(r - p);
		occlusion += occlusionFactor * ao.intensity * max(dot(rp, n), 0.f);

	}
	occlusion /= kKernelSize;
	float result = 1.f - occlusion;
	result = pow(result, ao.power);
	return result;
}
