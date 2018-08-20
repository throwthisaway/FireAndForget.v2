#include "../../../source/cpp/ShaderStructs.h"
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

static const float M_PI_F = 3.14159265f;

// https://learnopengl.com/PBR/Theory
float NDF_GGXTR(float3 n, float3 h, float roughness) {
	float roughness2 = roughness * roughness;
	float ndoth = max(dot(n, h), 0.f);
	float ndoth2 = ndoth * ndoth;
	float denom = ndoth2 * (roughness2 - 1.f) + 1.f;
	return roughness2 / (M_PI_F * denom * denom);
}

float GF_SchlickGGX(float ndotv, float k) {
	return ndotv / (ndotv * (1.f - k) + k);
}

float GF_Smith(float ndotv, float ndotl, float k) {
	float subNV = GF_SchlickGGX(ndotv, k);
	float subNL = GF_SchlickGGX(ndotl, k);
	return subNV * subNL;
}

float3 Fresnel_Schlick(float cosTheta, float3 f0) {
	return f0 + (1.f - f0) * pow(1.f - cosTheta, 5.f);
}

float3 WorldPosFormDepth(float2 uv, float4x4 ip, float depth) {
	float4 projected_pos = float4(uv * 2.f - 1.f, depth, 1.f);
	projected_pos.y = -projected_pos.y;
	float4 world_pos = mul(ip, projected_pos);
	return world_pos.xyz / world_pos.w;
}

float4 main(PSIn input) : SV_TARGET{
	float3 albedo = tRTT[0].Sample(smp, input.uv0).rgb;
	float3 n = Decode(tRTT[1].Sample(smp, input.uv0).xy);
	float4 material = tRTT[2].Sample(smp, input.uv0);
	float depth = tRTT[4].Sample(smp, input.uv0).r;
	// TODO:: better one with linear depth and without mat mult: https://mynameismjp.wordpress.com/2009/03/10/reconstructing-position-from-depth/
	float4 debug = tRTT[3].Sample(smp, input.uv0);
	float3 worldPos = WorldPosFormDepth(input.uv0, scene.ivp, depth);
	float3 v = normalize(scene.eyePos - worldPos);

	float roughness = material.r;
	float metallic = material.g;
	float3 f0 = float3(.04f, .04f, .04f);
	f0 = lerp(f0, albedo, metallic);
	float r = roughness + 1.f;
	float k = (r*r) / 8.f;
	float ndotv = max(dot(n, v), 0.f);

	float3 Lo = float3(0.f, 0.f, 0.f);
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		PointLight light = scene.light[i];
		float3 l = light.pos - worldPos;
		float l_distance = length(l);
		l /= l_distance;
		float3 h = normalize(v + l);
		float att = 1.f / (l_distance * l_distance);// TODO:: quadratic should be: 1.0f / dot(light.att, float3(1.0f, l_distance, l_distance * l_distance));
		float3 radiance = light.diffuse * att;

		float hdotv = clamp(dot(h, v), 0.f, 1.f);
		float3 F = Fresnel_Schlick(hdotv, f0);

		float NDF = NDF_GGXTR(n, h, roughness);

		float ndotl = max(dot(n, l), 0.f);
		float G = GF_Smith(ndotv, ndotl, k);

		float3 specular = (NDF * F * G) / max(4.f * ndotv * ndotl, 0.001f);

		float3 kS = F;
		float3 kD = float3(1.f, 1.f, 1.f) - kS;
		kD *= 1.f - metallic;
		Lo += (kD * albedo / M_PI_F + specular) * radiance * ndotl;
	}
	// TODO:: calc ao;
	float ao = 1.f;
	float3 ambient = float3(.03f, .03f, .03f) * albedo * ao;
	float3 color = ambient + Lo;
	// gamma correction
	color = color / (color + float3(1.f, 1.f, 1.f));
	float gamma = 1.f / 2.2f;
	color = pow(color, float3(gamma, gamma, gamma));
	return float4(color, 1.f);
}