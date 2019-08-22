#ifndef __SHADERSTRUCTS___
#define __SHADERSTRUCTS___
#ifndef MSVC_ALIGN16
// when used inside shaders
#define MSVC_ALIGN16
#endif
#ifndef CLANG_ALIGN16
// when used inside shaders
#define CLANG_ALIGN16
#endif

MSVC_ALIGN16 struct Object {
	float4x4 mvp, m;
};
MSVC_ALIGN16 struct Material {
	CLANG_ALIGN16 float3 diffuse;
	CLANG_ALIGN16 float2 metallic_roughness;
};
MSVC_ALIGN16 struct PointLight {
	CLANG_ALIGN16 float3 diffuse;
	CLANG_ALIGN16 float3 ambient;
	CLANG_ALIGN16 float3 specular;
	CLANG_ALIGN16 float3 pos;
	float4 att_range;
};
#define MAX_LIGHTS 2
MSVC_ALIGN16 struct SceneCB {
	float4x4 ip, ivp;
	PointLight light[MAX_LIGHTS];
	CLANG_ALIGN16 float3 eyePos;
	CLANG_ALIGN16 float2 nf;
	float2 viewport;
};
MSVC_ALIGN16 struct AO {
	float2 random_size;
	float rad;
	float scale;
	float intensity;
	float bias;
};
#endif
