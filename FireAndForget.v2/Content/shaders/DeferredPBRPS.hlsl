#include "../../../source/cpp/ShaderStructs.h"
#include "PSInput.hlsli"
#include "PI.hlsli"
#include "PBR.hlsli"
#include "Common.hlsli"
#include "DeferredRS.hlsli"
#include "DeferredBindings.hlsli"

ConstantBuffer<Scene> scene: register(b0);
ConstantBuffer<AO> ao : register(b1);
Texture2D tex[10] : register(t0);
SamplerState smp : register(s0);
SamplerState linearSmp : register(s1);
SamplerState linearClampSmp : register(s2);

float3 WorldPosFormDepth(float2 uv, float4x4 ip, float depth) {
	float4 projected_pos = float4(uv * 2.f - 1.f, depth, 1.f);
	projected_pos.y = -projected_pos.y;
	float4 world_pos = mul(ip, projected_pos);
	return world_pos.xyz / world_pos.w;
}
float AOPass(float2 uv, float3 p, float3 n, float4x4 ivp, Texture2D<float> depth) {
	float3 worldPos = WorldPosFormDepth(uv, ivp, depth.Sample(smp, uv).x);
	float3 diff = worldPos - p;
	float len = length(diff);
	float3 v = diff / len;
	len *= ao.scale;
	return max(0.f, dot(v, n) - ao.bias) * 1.f / (1.f + len) * ao.intensity;
}

#define ITERATION 4
float CalcAO(float2 uv, float3 center_pos, float3 n, float2 vp, float4x4 ivp, Texture2D<float> depth, Texture2D<float> random) {
	const float2 sampling[] = { float2(-1.f, 0.f), float2(1.f, 0.f), float2(0.f, 1.f), float2(0.f, -1.f) };

	float sum = 0.f, radius = ao.rad / center_pos.z;
	float2 rnd = normalize(random.sample(linearSmp, vp * uv / ao.random_size).xy * 2.f - 1.f);
	// TODO:: int iterations = lerp(6.0,2.0,p.z/g_far_clip);
	for (int i = 0; i < ITERATION; i++) {
		float2 c1 = reflect(sampling[i], rnd) * radius
		, c2 = float2(c1.x * .70716f - c1.y * .70716f,
					  c1.x * .70716f + c1.y * .70716f);

		sum += AOPass(uv + c1 * .25f, center_pos, n, ao, ivp, depth, smp);
		sum += AOPass(uv + c1 * .75f, center_pos, n, ao, ivp, depth, smp);
		sum += AOPass(uv + c2 * .5f, center_pos, n, ao, ivp, depth, smp);
		sum += AOPass(uv + c2, center_pos, n, ao, ivp, depth, smp);
	}
	sum /= (float)ITERATION * 4.f;
	return sum;
}

[RootSignature(DeferredRS)]
float4 main(PS_T input) : SV_TARGET{
	float3 albedo = tex[BINDING_ALBEDO].Sample(smp, input.uv).rgb;
	float3 n = Decode(tex[BINDING_NORMAL].Sample(smp, input.uv).xy);
	float4 material = tex[BINDING_MATERIAL].Sample(smp, input.uv);
	float depth = tex[BINDING_DEPTH].Sample(smp, input.uv).r;
	// TODO:: better one with linear depth and without mat mult: https://mynameismjp.wordpress.com/2009/03/10/reconstructing-position-from-depth/
	float4 debug = tex[BINDING_DEBUG].Sample(smp, input.uv);
	float3 worldPos = WorldPosFormDepth(input.uv, scene.ivp, depth);
	float3 v = normalize(scene.eyePos - worldPos);

	float roughness = material.r;
	float metallic = material.g;
	float3 f0 = float3(.04f, .04f, .04f);
	f0 = lerp(f0, albedo, metallic);
	float r = roughness + 1.f;
	float k = (r * r) / 8.f;
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
	float ao = CalcAO(input.uv, worldPos, n, scene.viewport, ao, scene.ivp, tex[BINDING_DOWNSAMPLE_DEPTH}, tex[BINDING_RANDOM);
	// 
	float3 f = Fresnel_Schlick_Roughness(ndotv, f0, roughness);
	float3 ks = f;
	float3 kd = 1.f - ks;
	kd *= 1.f - metallic;
	float3 irradiance = tex[BINDING_IRRADIANCE].Sample(linearSmp, n).rgb;
	float3 diffuse = irradiance * albedo.rgb;

	float3 r = reflect(-v, n);
	/*in the PBR fragment shader in line R = reflect(-V,N) - flip the sign of V.
	 Also I noticed author of this amazing article did't multiply reflected vector by inverse ModelView matrix. While it look fine in this example in a place where view matrix is rotated(with env cubemap) you'll really notice how it's going off.*/
	const float max_ref_lod = 4.f;	// TODO:: pass it as constant buffer
	float3 prefilerColor = tex[BINDING_PREFILTEREDENV].Sample(smp, r, level(roughness * max_ref_lod)).rgb;

	float2 envBRDF = tex[BINDING_BRDFLUT].Sample(linearClampSmp, float2(ndotv, roughness)).rg;
	float3 specular = prefilerColor * (f * envBRDF.x + envBRDF.y); // arleady multiplied by ks in Fresnel Shlick
	float3 ambient = (kd * diffuse + specular) - ao; // * aoResult;
	//
	//float3 ambient = albedo.rgb * ao * .03f;
	float3 color = ambient + Lo;
	// gamma correction
	color = color / (color + 1.f);
	color = pow(color, 1.f/2.2f);
	return float4(color, albedo.a);
}