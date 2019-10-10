#include <metal_stdlib>
using namespace metal;

float NDF_GGXTR(float3 n, float3 h, float roughness);
float NDF_GGXTR_a2(float ndoth, float a2);
float GF_Smith(float ndotv, float ndotl, float k);
float3 Fresnel_Schlick(float cosTheta, float3 f0);
float3 Fresnel_Schlick_Roughness(float hdotv, float3 f0, float roughness);
