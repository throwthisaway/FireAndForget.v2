#include "ShaderStructs.hlsl"
#include "Phong.hlsl"

Texture2D tColor : register(t0);

#define MAX_LIGHTS 2
cbuffer cMaterial : register(b0) {
	Material mat;
};
cbuffer cScene : register(b1) {
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
};

SamplerState smp : register(s0) {
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct PSIn {
	float4 pos : SV_POSITION;
	float4 world_pos : POSITION0;
	float3 n : NORMAL0;
	float2 uv0 : TEXCOORD0;
};

float4 main(PSIn input) : SV_TARGET {
	float4 diffuse_color = tColor.Sample(smp, input.uv0);
	float3 fragment = float3(0., 0., 0.);
	for (int i = 0; i < MAX_LIGHTS; i++) {
		fragment += ComputePointLight_Phong(light[i], eyePos, input.world_pos.xyz, input.n.xyz, diffuse_color.xyz, mat.specular, mat.power);
	}
	return float4(saturate(fragment), 1.f);
	//return tColor.Sample(smp, input.uv0);
}