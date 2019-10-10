#include <metal_stdlib>
using namespace metal;

struct FS_UV {
	float4 pos [[position]];
	float2 uv;
};
//
struct FS_P {
	float4 pos [[position]];
	float3 p [[user(position)]]; // localpos
};

struct FS_PN {
	float4 pos [[position]];
	float4 pWS [[user(position)]];
	float3 nWS [[user(normalWS)]];
	float3 nVS [[user(normalVS)]];
};

struct FS_UVN {
	float4 pos [[position]];
	float2 uv [[user(uv)]];
	float3 nWS [[user(normalWS)]];
	float3 nVS [[user(normalVS)]];
};

struct FS_UVNT {
	float4 pos [[position]];
	float2 uv [[user(texcoord0)]];
	float3 nWS [[user(normalWS)]];
	float3 nVS [[user(normalVS)]];
	float3 tWS [[user(tangentWS)]];
};
