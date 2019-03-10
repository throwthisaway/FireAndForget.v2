#include <metal_stdlib>
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

// from CubeEnvMap.metal
struct FSIn {
	float4 pos [[position]];
	float3 p; // localpos
};
fragment float4 prefilter_fs_main(FSIn input [[stage_in]],
								  constant float& roughness [[buffer(0)]],
								  texturecube<float> env [[texture(0)]],
								  sampler smp [[sampler(0)]]) {
	const uint sampleCount = 1024u;
	const float a = roughness * roughness,
				a2 = a*a,
				a2minus1 = a2 - 1.f;


	const float3 n = normalize(input.p);
	const float3 r = n;
	const float3 v = r;

	const float3 up = (abs(n.z) < .999f) ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	const float3 tangent = normalize(cross(up, n));
	const float3 bitangent = cross(n, tangent);
	const float3x3 sphericalToTangent(tangent, bitangent, n);

	float3 prefilteredColor = float3(0.f);
	float weight = 0.f;
	for (uint i = 0; i < sampleCount; ++i) {
		float2 xi = Hammersley(i, sampleCount);
		float3 h = ImportanceSampleGGX(xi, sphericalToTangent, a2minus1);
		float3 l = normalize(2.f * dot(v, h) * h - v);

		float ndotl = dot(n, l);
		if (ndotl > 0.f) {
			prefilteredColor += env.sample(smp, l).rgb * ndotl;
			weight += ndotl;
		}
	}
	return float4(prefilteredColor / weight, 1.f);
}
