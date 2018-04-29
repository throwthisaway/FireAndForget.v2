#include <metal_stdlib>
using namespace metal;

struct FragOut {
	float4 albedo [[color(0)]];
	float2 normal [[color(1)]];
	float4 material [[color(2)]];
	float4 debug [[color(3)]];
};

float2 Encode(float3 v);
float3 Decode(float2 v);
