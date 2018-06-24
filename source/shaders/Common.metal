#include "Common_include.metal"
using namespace metal;
// as seen on https://aras-p.info/texts/CompactNormalStorage.html
// Method #4

float2 Encode(float3 v) {
	float p = sqrt(v.z * 8.f + 8.f);
	return float2(v.xy/p + .5f);
}

float3 Decode(float2 v) {
	float2 fenc = v * 4.f - 2.f;
	float f = dot(fenc,fenc);
	float g = sqrt(1.f - f / 4.f);
	float3 n;
	n.xy = fenc*g;
	n.z = 1.f - f/2.f;
	return n;
}

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
