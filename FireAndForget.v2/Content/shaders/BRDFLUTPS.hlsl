#include "PSInput.hlsli"
#include "Hammersley.hlsli"
#include "ImportanceSampleGGX.hlsli"
#include "BRDFLUTRS.hlsli"
#include "PBR.hlsli"

float2 IntegrateBRDF(float ndotv, float roughness) {
	const float3 v = float3(sqrt(1.f - ndotv * ndotv), 0.f, ndotv);
	const float3 n = float3(0.f, 0.f, 1.f);

	const float3 up = (abs(n.z) < .999f) ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	const float3 tangent = normalize(cross(up, n));
	const float3 bitangent = cross(n, tangent);
	const float3x3 sphericalToTangent = float3x3(tangent, bitangent, n);

	// TODO:: extract to cb
	const float a = roughness * roughness,
		a2 = a*a,
		a2minus1 = a2 - 1.f;
	// https://learnopengl.com/PBR/IBL/Specular-IBL
	//Note that while k takes a as its parameter we didn't square roughness as a as we originally did for other interpretations of a; likely as a is squared here already. I'm not sure whether this is an inconsistency on Epic Games' part or the original Disney paper, but directly translating roughness to a gives the BRDF integration map that is identical to Epic Games' version.
	const float k = a / 2.f; // IBL TODO:: why not a2 / 2.f ???
	float2 res = (float2)0.f;
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
[RootSignature(BRDFLUTRS)]
float2 main(PS_T input) : SV_TARGET {
	return IntegrateBRDF(input.uv.x, input.uv.y);
}