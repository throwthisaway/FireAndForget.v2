#include "../../../source/cpp/ShaderStructs.h"
#include "Phong.hlsli"
#include "Common.hlsli"

cbuffer cScene : register(b0) {
	Scene scene;
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
float3 WorldPosFormDepth(float2 uv, float4x4 ip, float depth) {
	float4 projected_pos = float4(uv * 2.f - 1.f, depth, 1.f);
	projected_pos.y = -projected_pos.y;
	float4 world_pos = mul(ip, projected_pos);
	return world_pos.xyz / world_pos.w;
}
float4 main(PSIn input) : SV_TARGET {
	float3 diffuseColor = tRTT[0].Sample(smp, input.uv0).rgb;
	float3 n = Decode(tRTT[1].Sample(smp, input.uv0).xy);
	float4 mat = tRTT[2].Sample(smp, input.uv0);
	float depth = tRTT[4].Sample(smp, input.uv0).r;
	// TODO:: better one with linear depth and without mat mult: https://mynameismjp.wordpress.com/2009/03/10/reconstructing-position-from-depth/
	float3 world_pos = WorldPosFormDepth(input.uv0, scene.ip, depth);

	float3 diff = float3(0.f, 0.f, 0.f);
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		diff += ComputePointLight_Diffuse(scene.light[i],
			world_pos,
			n,
			diffuseColor);
		diff += ComputePointLight_Phong(scene.light[i],
			scene.eyePos,
			world_pos,
			n,
			diffuseColor,
			mat.x/*specular*/,
			mat.y/*power*/);
	}
	return float4(diff, 1.f);
}