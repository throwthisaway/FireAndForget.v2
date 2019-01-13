#include <metal_stdlib>
#include <metal_math>
#include "Common.h.metal"
#include "PBR.h.metal"


struct PosUV {
	packed_float2 pos;
	packed_float2 uv;
};

struct FSIn {
	float4 pos [[position]];
	float2 uv; // TODO:: ???[[]];
};

vertex FSIn deferred_pbr_vs_main(constant PosUV* input [[buffer(0)]],
						uint id [[vertex_id]]) {
	FSIn res;
	res.pos = float4(input[id].pos, 0.f, 1.f);
	res.uv = input[id].uv;
	return res;
}

struct DeferredOut {
	float4 frag [[color(0)]];
	float4 debug [[color(1)]];
};
fragment DeferredOut deferred_pbr_fs_main(FSIn input [[stage_in]],
									  constant Scene& scene [[buffer(0)]],
									  texture2d<float> albedoTx [[texture(0)]],
									  texture2d<float> normal [[texture(1)]],
									  texture2d<float> materialTx [[texture(2)]],
									  texture2d<float> debug [[texture(3)]],
									  texture2d<float> depth [[texture(4)]],
									  texturecube<float> irradianceTx [[texture(5)]],
									  sampler smp [[sampler(0)]]) {
	float3 worldPos = WorldPosFormDepth(input.uv, scene.ivp, depth.sample(smp, input.uv).x);
	//float3 worldPos = debug.sample(smp, input.uv).xyz;
	float3 n = Decode(normal.sample(smp, input.uv).xy);
	float4 material = materialTx.sample(smp, input.uv);
	float4 albedo = albedoTx.sample(smp, input.uv);
	float3 v = normalize(scene.eyePos - worldPos);

	float roughness = material.r;
	float metallic = material.g;
	float3 f0 = float3(.04f);
	f0 = mix(f0, albedo.rgb, metallic);
	float r = roughness + 1.f;
	float k = (r*r) / 8.f;
	float ndotv = max(dot(n, v), 0.f);

	float3 Lo = 0.f;
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		auto light = scene.light[i];
		float3 l = light.pos - worldPos;
		float l_distance = length(l);
		l /= l_distance;
		float3 h = normalize(v + l);
		float att = 1.f / (l_distance * l_distance);// TODO:: quadratic should be: 1.0f / dot(light.att, float3(1.0f, l_distance, l_distance * l_distance));
		float3 radiance = light.diffuse * att;

		float hdotv = max(dot(h, v), 0.f);
		float3 F = Fresnel_Schlick(hdotv, f0);

		float NDF = NDF_GGXTR(n, h, roughness);

		float ndotl = max(dot(n, l), 0.f);
		float G = GF_Smith(ndotv, ndotl, k);

		float3 specular = (NDF * F * G) / max(4.f * ndotv * ndotl, 0.001f);

		float3 kS = F;
		float3 kD = 1.f - kS;
		kD *= 1.f - metallic;
		Lo += (kD * albedo.rgb / M_PI_F + specular) * radiance * ndotl;
	}
	DeferredOut res;
	// TODO:: calc ao;
	float ao = 1.f;
	float3 kS = Fresnel_Schlick_Roughness(ndotv, f0, roughness);
	float3 kD = 1.f - kS;
	float3 irradiance = irradianceTx.sample(smp, n).rgb;
	float3 diffuse = irradiance * albedo.rgb;
	float3 ambient = kD * diffuse * ao;
	//float3 ambient = albedo.rgb * ao * .03f;
	float3 color = ambient + Lo;
	// gamma correction
	color = color / (color + 1.f);
	color = pow(color, 1.f/2.2f);
	res.frag = float4(color, albedo.a);
	float4 debug_n = debug.sample(smp, input.uv);
	res.debug = debug_n;
	//res.frag = float4(float3(debug_n), 1.f);
	return res;
}
