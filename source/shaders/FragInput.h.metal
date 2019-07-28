#include <metal_stdlib>
using namespace metal;

struct FragT {
	float4 pos [[position]];
	float2 uv;
};

struct FragP {
	float4 pos [[position]];
	float3 p; // localpos
};
