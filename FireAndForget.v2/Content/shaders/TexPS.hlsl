Texture2D diffuse : register(t0);

struct PointLight {
	float4 diffuse;
	float4 ambient;
	float4 specular;
	float3 pos;
	float3 att;
	float range;
};

struct Material {
	float3 diffuse;
	float specular, power;
};

#define MAX_LIGHTS 2
cbuffer cObject : register(b0) {
	Material mat;
};
cbuffer cFrame : register(b1) {
	PointLight light[MAX_LIGHTS];
	float4 eyePos;
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

float3 ComputePointLight_Phong(PointLight l, float3 pos, float3 normal, float3 col, float spec, float power) {
	float3 lv = l.pos - pos;	// vector towards light
	float l_distance = length(lv);
	if (l_distance > l.range) return float3(.0, .0, .0);

	lv = lv / l_distance;			// light direction vector

	normal = normalize(normal);// TODO:: normalize(n) needed?
	float l_intensity = dot(lv, normal);	// light intensity
	if (l_intensity > 0.) {
		float3 diffuse = l_intensity * l.diffuse * col;	// diffuse term
														// specular term
		float3 r = -reflect(lv, normal);	// light reflection driection from the surface
		float3 v = normalize(eyePos.xyz - pos);	//normalized eye direction vector from the surface
		float3 specular = l.specular.xyz * pow(saturate(dot(r, v)), power) * spec;
		//attenuation
		float att = 1.0f / dot(l.att, float3(1.0f, l_distance, l_distance * l_distance));
		return (diffuse + specular) * att;
	}
	return float3(.0, .0, .0);// TODO:: ambient
}
float3 ComputePointLight_BlinnPhong(PointLight l, float3 pos, float3 normal, float3 col, float spec, float power) {
	float3 lv = l.pos - pos;	// vector towards light
	float l_distance = length(lv);
	if (l_distance > l.range) return float3(.0, .0, .0);

	lv = lv / l_distance;			// light direction vector
	normal = normalize(normal);	// TODO:: normalize(n) needed?
	float l_intensity = dot(lv, normal);	// light intensity
	if (l_intensity > 0.) {
		float3 diffuse = l_intensity * l.diffuse * col;	// diffuse term
		float3 v = normalize(eyePos.xyz - pos);	//normalized eye direction vector from the surface
		float3 h = normalize(lv + v);	// half-vector, *.5 instead of normalize

		float3 specular = l.specular.xyz * pow(saturate(dot(h, normal)), power) * spec;
		//attenuation
		float att = 1.0f / dot(l.att, float3(1.0f, l_distance, l_distance * l_distance));
		return (diffuse + specular) * att;
	}
	return float3(.0, .0, .0);// TODO:: ambient
}
float3 ComputePointLight_Diffuse(PointLight l, float3 pos, float3 normal, float3 col, float spec, float power) {
	float3 lv = l.pos - pos;	// vector towards light
	float l_distance = length(lv);
	//	if (l_distance > l.range) return float3(.0, .0, .0);
	//attenuation
	return  max(dot(lv / l_distance, normalize(normal)), 0.f) * l.diffuse * col / dot(l.att, float3(1.0f, l_distance, l_distance * l_distance));
}
float4 main(PSIn input) : SV_TARGET {
	float4 diffuse_color = diffuse.Sample(smp, input.uv0);
	float3 fragment = float3(0., 0., 0.);
	for (int i = 0; i < MAX_LIGHTS; i++) {
		fragment += ComputePointLight_Phong(light[i], input.world_pos.xyz, input.n.xyz, diffuse_color.xyz, mat.specular, mat.power);
	}
	return float4(saturate(fragment), 1.f);
}