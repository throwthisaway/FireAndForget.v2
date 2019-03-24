#include <metal_stdlib>
#include <metal_math>
#include "Common.h.metal"
#include "PBR.h.metal"

struct FSIn {
	float4 pos [[position]];
	float2 uv; // TODO:: ???[[]];
};


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
									  texturecube<float> prefilteredEnvTx [[texture(6)]],
									  texture2d<float> BRDFLUT [[texture(7)]],
									  sampler deferredsmp [[sampler(0)]],
									  sampler linearsmp [[sampler(1)]],
									  sampler mipmapsmp [[sampler(2)]],
									  sampler clampsmp [[sampler(3)]]) {
	float3 worldPos = WorldPosFormDepth(input.uv, scene.ivp, depth.sample(deferredsmp, input.uv).x);
	//float3 worldPos = debug.sample(deferredsmp, input.uv).xyz;
	float3 n = Decode(normal.sample(deferredsmp, input.uv).xy);
	float4 material = materialTx.sample(deferredsmp, input.uv);
	float4 albedo = albedoTx.sample(deferredsmp, input.uv);
	float3 v = normalize(scene.eyePos - worldPos);

	const float roughness = material.r;
	const float metallic = material.g;
	float3 f0 = float3(.04f);
	f0 = mix(f0, albedo.rgb, metallic);
	float r1 = roughness + 1.f;
	float k = (r1*r1) / 8.f;
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
	//
	float3 f = Fresnel_Schlick_Roughness(ndotv, f0, roughness);
	float3 ks = f;
	float3 kd = 1.f - ks;
	kd *= 1.f - metallic;
	float3 irradiance = irradianceTx.sample(linearsmp, n).rgb;
	float3 diffuse = irradiance * albedo.rgb;

	float3 r = reflect(-v, n);
	/*in the PBR fragment shader in line R = reflect(-V,N) - flip the sign of V.
	 Also I noticed autor of this amazing article did't multiply reflected vector by inverse ModelView matrix. While it look fine in this example in a place where view matrix is rotated(with env cubemap) you'll really notice how it's going off.*/
	const float max_ref_lod = 4.f;	// TODO:: pass it as constant buffer
	float3 prefilerColor = prefilteredEnvTx.sample(mipmapsmp, r, level(roughness * max_ref_lod)).rgb;

	float2 envBRDF = BRDFLUT.sample(clampsmp, float2(ndotv, roughness)).rg;
	float3 specular = prefilerColor * (f * envBRDF.x + envBRDF.y); // arleady multiplied by ks in Fresnek Shlick
	float3 ambient = (kd * diffuse + specular) * ao;
	//
	//float3 ambient = albedo.rgb * ao * .03f;
	float3 color = ambient + Lo;
	// gamma correction
	color = color / (color + 1.f);
	color = pow(color, 1.f/2.2f);
	res.frag = float4(color, albedo.a);
//	res.frag = float4(irradiance, albedo.a);
	float4 debug_n = debug.sample(linearsmp, input.uv);
	res.debug = debug_n;
	//res.frag = float4(float3(debug_n), 1.f);
	return res;
}
