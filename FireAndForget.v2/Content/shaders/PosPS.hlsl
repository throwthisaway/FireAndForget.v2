#include "ShaderStructs.hlsl"
#include "Phong.hlsl"

#define MAX_LIGHTS 2
cbuffer cMaterial : register(b0) {
	Material mat;
};
cbuffer cScene : register(b1) {
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
};

struct PSIn {
	float4 pos : SV_POSITION;
	float4 world_pos : POSITION0;
	float3 n : NORMAL0;
};

float4 main(PSIn input) : SV_TARGET{
	float3 fragment = float3(0., 0., 0.);
	for (int i = 0; i < MAX_LIGHTS; i++) {
		fragment += ComputePointLight_Phong(light[i], eyePos, input.world_pos.xyz, input.n.xyz, mat.diffuse, mat.specular, mat.power);
	}
	return float4(saturate(fragment), 1.f);
}