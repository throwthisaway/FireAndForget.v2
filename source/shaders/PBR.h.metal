#include <metal_stdlib>
using namespace metal;
#include "../cpp/ShaderStructs.h"

float NDF_GGXTR(float3 n, float3 h, float roughness);
float NDF_GGXTR_a2(float ndoth, float a2);
float GF_Smith(float ndotv, float ndotl, float k);
float3 Fresnel_Schlick(float cosTheta, float3 f0);
float3 Fresnel_Schlick_Roughness(float hdotv, float3 f0, float roughness);
struct PBRMaterial {
	float3 albedo;
	float metallic;
	float roughness;
	float ao;
};
float3 ComputePointLight_PBR(PointLight l, float3 p, float3 n, PBRMaterial material);
