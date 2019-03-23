#include <metal_stdlib>
#include "PBR.h.metal"
#include "FragInput.h.metal"

using namespace metal;

// https://learnopengl.com/PBR/IBL/Specular-IBL
float RadicalInverse_VdC(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}
// ----------------------------------------------------------------------------
float2 Hammersley(uint i, uint count) {
	return float2(float(i)/float(count), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 xi, float3x3 mat/*spherical to cartesian*/, const float a2minus1/*a*a-1*/) {
	float phi = 2.f * M_PI_F * xi.x;
	float cosTheta = sqrt((1.f - xi.y) / (1.f + a2minus1 * xi.y));	// TODO:: why???
	float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	float3 h(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	return normalize(mat * h);
}

fragment float4 prefilter_fs_main(FragP input [[stage_in]],
								  constant float& roughness [[buffer(0)]],
								  constant float& resolution [[buffer(1)]],
								  texturecube<float> env [[texture(0)]],
								  sampler smp [[sampler(0)]]) {

	const float3 n = normalize(input.p);
	const float3 r = n;
	const float3 v = r;

	const float3 up = (abs(n.z) < .999f) ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	const float3 tangent = normalize(cross(up, n));
	const float3 bitangent = cross(n, tangent);
	const float3x3 sphericalToTangent(tangent, bitangent, n);

	const float a = roughness * roughness,
	a2 = a*a,
	a2minus1 = a2 - 1.f;

	float3 prefilteredColor = float3(0.f);
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

			prefilteredColor += env.sample(smp, l, level(mipLevel)).rgb * ndotl;
			weight += ndotl;
		}
	}
	return float4(prefilteredColor / weight, 1.f);
}

float2 IntegrateBRDF(float ndotv, float roughness) {
	const float3 v = float3(sqrt(1.f - ndotv * ndotv), 0.f, ndotv);
	const float3 n = float3(0.f, 0.f, 1.f);

	const float3 up = (abs(n.z) < .999f) ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	const float3 tangent = normalize(cross(up, n));
	const float3 bitangent = cross(n, tangent);
	const float3x3 sphericalToTangent(tangent, bitangent, n);

	const float a = roughness * roughness,
		a2 = a*a,
		a2minus1 = a2 - 1.f;
	// https://learnopengl.com/PBR/IBL/Specular-IBL
	//Note that while k takes a as its parameter we didn't square roughness as a as we originally did for other interpretations of a; likely as a is squared here already. I'm not sure whether this is an inconsistency on Epic Games' part or the original Disney paper, but directly translating roughness to a gives the BRDF integration map that is identical to Epic Games' version.
	const float k = a / 2.f; // IBL TODO:: why not a2 / 2.f ???
	float2 res = float2(0.f);
	const uint sampleCount = 1024u;
	for (uint i = 0; i < sampleCount; ++i) {
		float2 xi = Hammersley(i, sampleCount);
		float3 h = ImportanceSampleGGX(xi, sphericalToTangent, a2minus1);
		float vdoth = dot(v, h);
		float3 l = normalize(2.f * vdoth * h - v);

		float ndotl = l.z; // ???
		if (ndotl > 0.f) {
			float g = GF_Smith(ndotv, ndotl, k);
			float ndoth = h.z; // ???
			vdoth = max(vdoth, 0.f);
			float gvis = (g * vdoth) / (ndoth * ndotv); // ???
			float fc = pow(1.f - vdoth, 5.f);

			float fc_gvis = fc * gvis;
			res.x += gvis - fc_gvis; // ==> (1.f - fc) * gvis;
			res.y += fc_gvis;
		}
	}
	return res / sampleCount;
}
fragment float2 integratebrdf_fs_main(FragT input [[stage_in]]) {
	return IntegrateBRDF(input.uv.x, input.uv.y);
}
