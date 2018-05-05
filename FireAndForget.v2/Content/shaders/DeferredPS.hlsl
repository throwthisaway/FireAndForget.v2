#include "ShaderStructs.hlsl"
#include "Phong.hlsl"

#define MAX_LIGHTS 2
struct cScene {
	float4x4 ip;
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
	float n, f;
};
Texture2D tRTT[5] : register(t0);

SamplerState smp : register(s0) {
	Filter = MIN_MAG_MIP_NEAREST;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct PSIn {
	float4 pos : SV_POSITION;
	float2 uv0 : TEXCOORD0;
};
//float3 WorldPosFormDepth(float2 uv, float4x4 ip, texture2d<float> depth, sampler smp) {
//	float4 projected_pos = float4(uv * 2.f - 1.f, depth.sample(smp, uv).x, 1.f);
//	projected_pos.y = -projected_pos.y;
//	float4 world_pos = ip * projected_pos;
//	return world_pos.xyz / world_pos.w;
//}
float4 main(PSIn input) : SV_TARGET {
	return tRTT[0].Sample(smp, input.uv0);
}