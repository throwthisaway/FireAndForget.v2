#include <metal_stdlib>
#include <metal_math>
#include "PBR_include.metal"
using namespace metal;

// https://learnopengl.com/PBR/Theory
float NDF_GGXTR(float3 n, float3 h, float roughness) {
	float roughness2 = roughness * roughness;
	float ndoth = max(dot(n, h), 0.f);
	float ndoth2 = ndoth * ndoth;
	float denom = ndoth2 * (roughness2 - 1.f) + 1.f;
	return roughness2 / (M_PI_F * denom * denom);
}

float GF_SchlickGGX(float ndotv, float k) {
	return ndotv / (ndotv * k);
}

float GF_Smith(float ndotv, float ndotl, float k) {
	float subNV = GF_SchlickGGX(ndotv, k);
	float subNL = GF_SchlickGGX(ndotl, k);
	return subNV * subNL;
}

float3 Fresnel_Schlick(float cosTheta, float3 f0) {
	return f0 + (1.f - f0) * pow(1.f - cosTheta, 5.f);
}
