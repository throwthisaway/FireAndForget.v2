#ifndef __SHADERSTRUCTS___
#define __SHADERSTRUCTS___
struct Object {
	float4x4 mvp;
	float4x4 m;
};
struct Material {
	float3 diffuse;
	float pad1;
	float specular, power;
};
struct PointLight {
	float3 diffuse;
	float pad1;
	float3 ambient;
	float pad2;
	float3 specular;
	float pad3;
	float3 pos;
	float pad4;
	float3 att;
	float pad5;
	float range;
	float3 pad6;
};
#define MAX_LIGHTS 2
struct Scene {
	float4x4 ip, ivp;
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
	float pad0;
	float n, f;
};
#endif
