#include <metal_stdlib>
using namespace metal;

struct MRTOut {
	float4 albedo [[color(0)]];
	float4 normalWS [[color(1)]];
	float4 normalVS [[color(2)]];
	float4 material [[color(3)]];
	float4 debug [[color(4)]];
};

float LinearizeDepth(float depth, float n, float f);
float3 WorldPosFormDepth(float2 uv, float4x4 ip, float depth);
