#ifndef __SHADERSTRUCTS___
#define __SHADERSTRUCTS___

struct Object {
	// https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-per-component-math?redirectedfrom=MSDN#Matrix_Ordering
	// or use #pragmapack_matrix directive in shader
	ALIGN16 row_major float4x4 mvp, m, mv;
};
struct Material {
	ALIGN16 float3 diffuse;
	ALIGN16 float2 metallic_roughness;
};
struct PointLight {
	ALIGN16 float3 diffuse;
	ALIGN16 float3 ambient;
	ALIGN16 float3 specular;
	ALIGN16 float3 pos;
	ALIGN16 float4 att_range;
};
struct ShadowMap {
	ALIGN16 row_major float4x4 vpt;
};
#define MAX_LIGHTS 2
#define MAX_SHADOWMAPS 1
struct SceneCB {
	ALIGN16 float4x4 ip, ivp;
	ALIGN16 PointLight light[MAX_LIGHTS];
	ALIGN16 ShadowMap shadowMaps[MAX_SHADOWMAPS];
	ALIGN16 float3 eyePos;
	ALIGN16 float2 nf;
	ALIGN16 float2 viewport;
};
struct ALIGN16 SSAOScene {
	ALIGN16 row_major float4x4 proj, ivp, ip, view;
	ALIGN16 float2 viewport;
};
struct ALIGN16 AO {
	float2 randomFactor;
	float rad;
	float scale;
	float intensity;
	float fadeStart, fadeEnd;
	float bias;
	float power;
};
#endif
