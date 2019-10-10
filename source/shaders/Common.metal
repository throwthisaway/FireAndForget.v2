#include "Common.h.metal"
using namespace metal;

float LinearizeDepth(float depth, float n, float f) {
	return (2.0 * n) / (f + n - depth * (f - n));
}

float3 WorldPosFormDepth(float2 uv, float4x4 ip, float depth) {
	// TODO:: better one with linear depth and without mat mult: https://mynameismjp.wordpress.com/2009/03/10/reconstructing-position-from-depth/
	float4 projected_pos = float4(uv * 2.f - 1.f, depth, 1.f);
	projected_pos.y = -projected_pos.y;
	float4 world_pos = ip * projected_pos;
	return world_pos.xyz / world_pos.w;
}
