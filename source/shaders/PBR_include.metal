#include <metal_stdlib>
#include "ShaderStructs.metal"
using namespace metal;
float NDF_GGXTR(float3 n, float3 h, float roughness);
float GF_Smith(float ndotv, float ndotl, float k);
float3 Fresnel_Schlick(float cosTheta, float3 f0);
struct PBRMaterial {
	float3 albedo;
	float metallic;
	float roughness;
	float ao;
};
float3 ComputePointLight_PBR(PointLight l, float3 p, float3 n, PBRMaterial material);
