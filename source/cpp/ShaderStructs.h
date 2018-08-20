#ifndef __SHADERSTRUCTS___
#define __SHADERSTRUCTS___
#ifndef ALIGN16
#define ALIGN16
#endif

ALIGN16 struct Object {
	float4x4 mvp;
	float4x4 m;
};
ALIGN16 struct Material {
	float3 diffuse;
	float specular, power;
};
ALIGN16 struct PointLight {
	float3 diffuse;
	float3 ambient;
	float3 specular;
	float3 pos;
	float3 att;
	float range;
};
#define MAX_LIGHTS 2
ALIGN16 struct Scene {
	float4x4 ip, ivp;
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
	float n, f;
};
#endif
