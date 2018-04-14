#include "ShaderStructs.metal"
using namespace metal;
float3 ComputePointLight_Diffuse(PointLight l, float3 pos, float3 normal, float3 col);
