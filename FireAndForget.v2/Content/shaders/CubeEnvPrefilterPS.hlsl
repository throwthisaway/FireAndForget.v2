#include "PSInput.hlsli"
#include "Hammersley.hlsli"
#include "ImportanceSampleGGX.hlsli"
#include "PBR.hlsli"
#include "CubeEnvPrefilterRS.hlsli"

cbuffer cb0 : register(b0) {
	float roughness;
}
cbuffer cb1 : register(b1) {
	float resolution;
}
TextureCube<float4> envMap : register(t0);
SamplerState smp : register(s0);

[RootSignature(CubeEnvPrefilterRS)]
float4 main(PS_P input) : SV_TARGET {
	const float3 n = normalize(input.p);
	const float3 r = n;
	const float3 v = r;

	const float3 up = (abs(n.z) < .999f) ? float3(0.f, 0.f, 1.f) : float3(0.f, 1.f, 0.f);
	const float3 tangent = normalize(cross(up, n));
	const float3 bitangent = cross(n, tangent);
	const float3x3 sphericalToTangent = float3x3(tangent, bitangent, n);

	// TODO:: extract these to cb
	const float a = roughness * roughness,
		a2 = a*a,
		a2minus1 = a2 - 1.f;

	float3 prefilteredColor = (float3)0.f;
	float weight = 0.f;
	const uint sampleCount = 1024u;
	for (uint i = 0; i < sampleCount; ++i) {
		float2 xi = Hammersley(i, sampleCount);
		float3 h = ImportanceSampleGGX(xi, sphericalToTangent, a2minus1);
		float vdoth = dot(v, h);
		float3 l = normalize(2.f * vdoth * h - v);

		float ndotl = dot(n, l);
		if (ndotl > 0.f) {
			float ndoth = max(dot(n, h), 0.f);
			float d = NDF_GGXTR_a2(ndoth, a2);
			float pdf = (d * ndoth / (4.f * vdoth)) + .0001f;
			float saTexel  = 4.f * M_PI_F / (6.f * resolution * resolution);
			float saSample = 1.f / sampleCount * pdf + .0001f;

			float mipLevel = roughness == 0.f ? 0.f : .5f * log2(saSample / saTexel);

			prefilteredColor += envMap.SampleLevel(smp, l, mipLevel).rgb * ndotl;
			weight += ndotl;
		}
	}
	return float4(prefilteredColor / weight, 1.f);
}