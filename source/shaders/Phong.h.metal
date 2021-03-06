#include <metal_stdlib>
#include "ShaderStructs.h.metal"
using namespace metal;
float3 ComputePointLight_Diffuse(PointLight l, float3 pos, float3 normal, float3 col);
float3 ComputePointLight_BlinnPhong(PointLight l, float3 eyePos, float3 pos, float3 normal, float3 col, float spec, float power);
float3 ComputePointLight_Phong(PointLight l, float3 eyePos, float3 pos, float3 normal, float3 col, float spec, float power);

