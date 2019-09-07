#ifndef __SHADERSTRUCTS___
#define __SHADERSTRUCTS___

struct Object {
	float4x4 mvp, m;
};
struct Material {
	float3 diffuse;
	float2 metallic_roughness;
};
struct PointLight {
	float3 diffuse;
	float3 ambient;
	float3 specular;
	float3 pos;
	float4 att_range;
};
#define MAX_LIGHTS 2
struct SceneCB {
	float4x4 ip, ivp;
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
	float2 nf;
	float2 viewport;
};
struct SSAOScene {
	float4x4 proj;
	float2 viewport;
};
struct AO {
	float2 random_size;
	float rad;
	float scale;
	float intensity;
	float bias;
};
#endif
