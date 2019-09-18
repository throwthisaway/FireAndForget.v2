#include "ShaderStructs.h"
#include "ShaderInput.hlsli"
#include "Common.hlsli"
#include "PBR.hlsli"
#include "DeferredRS.hlsli"

ConstantBuffer<SceneCB> scene: register(b0);
Texture2D<float4> texAlbedo : register(t0);
Texture2D<float4> texNormal : register(t1);
Texture2D<float4> texMaterial : register(t2);
#ifdef DEBUG_RT
Texture2D<float4> texDebug : register(t3);
#endif
Texture2D<float> texDepth : register(t4);
TextureCube<float4> texIrradiance : register(t5);
TextureCube<float4> texPrefilteredEnv : register(t6);
Texture2D<float2> texBRDFLUT : register(t7);
Texture2D<float> texSSAO : register(t8);
Texture2D<float4> texSSAODebug : register(t9);
SamplerState smp : register(s0);
SamplerState linearSmp : register(s1);
SamplerState linearClampSmp : register(s2);

float3 WorldPosFormDepth(float2 uv, float4x4 ip, float depth) {
	float4 projected_pos = float4(uv * 2.f - 1.f, depth, 1.f);
	projected_pos.y = -projected_pos.y;
	float4 world_pos = mul(ip, projected_pos);
	return world_pos.xyz / world_pos.w;
}

[RootSignature(DeferredRS)]
float4 main(PS_UV input) : SV_TARGET{
	float4 albedo = texAlbedo.Sample(smp, input.uv);
	float4 debug = texDebug.Sample(smp, input.uv);
	float3 n = normalize(texNormal.Sample(smp, input.uv).rgb);
	float4 material = texMaterial.Sample(smp, input.uv);
	float depth = texDepth.Sample(smp, input.uv).r;
	// TODO:: better one with linear depth and without mat mult: https://mynameismjp.wordpress.com/2009/03/10/reconstructing-position-from-depth/
	float3 worldPos = WorldPosFormDepth(input.uv, scene.ivp, depth);
	float3 v = normalize(scene.eyePos - worldPos);

	float metallic = material.r;
	float roughness = material.g;
	float3 f0 = float3(.04f, .04f, .04f);
	f0 = lerp(f0, albedo.rgb, metallic);
	float r1 = roughness + 1.f;
	float k = (r1 * r1) / 8.f;
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
		Lo += (kD * albedo.rgb / M_PI_F + specular) * radiance * ndotl;
	}
	float ao = texSSAO.Sample(linearSmp, input.uv).r;
	// 
	float3 f = Fresnel_Schlick_Roughness(ndotv, f0, roughness);
	float3 ks = f;
	float3 kd = 1.f - ks;
	kd *= 1.f - metallic;
	float3 irradiance = texIrradiance.Sample(linearSmp, n).rgb;
	float3 diffuse = irradiance * albedo.rgb;

	float3 r = reflect(-v, n);
	/*in the PBR fragment shader in line R = reflect(-V,N) - flip the sign of V.
	 Also I noticed author of this amazing article did't multiply reflected vector by inverse ModelView matrix. While it look fine in this example in a place where view matrix is rotated(with env cubemap) you'll really notice how it's going off.*/
	const float max_ref_lod = 4.f;	// TODO:: pass it as constant buffer
	float3 prefilterColor = texPrefilteredEnv.SampleLevel(linearSmp, r, roughness * max_ref_lod).rgb;

	float2 envBRDF = texBRDFLUT.Sample(linearClampSmp, float2(ndotv, roughness)).rg;
	float3 specular = prefilterColor * (f * envBRDF.x + envBRDF.y); // arleady multiplied by ks in Fresnel Shlick
	float3 ambient = (kd * diffuse + specular) * ao;
	Lo *= ao;
	//
	//float3 ambient = albedo.rgb * ao * .03f;
	float3 color = ambient + Lo;
	return float4(GammaCorrection(color), albedo.a);
	//return float4(ao, ao, ao, 1.f);
	//float4 ssaoDebug = texSSAODebug.Sample(linearClampSmp, input.uv);
	//return float4(ssaoDebug.xyz, 1.f);
	//float4 vp = mul(scene.ip, debug);
	//return float4(vp.xyz, 1.f);
	/*float3 l = normalize(scene.light[0].pos - worldPos);
	float d = max(0.f, dot(debug.rgb, l));
	return float4(d, d, d, albedo.a);*/
}