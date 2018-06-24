#include <metal_stdlib>
#include "Common_include.metal"
#include "Phong_include.metal"
using namespace metal;

struct PosUV {
	float2 pos;
	float2 uv;
};

struct FSIn {
	float4 pos [[position]];
	float2 uv; // TODO:: ???[[]];
};

vertex FSIn deferred_vs_main(constant PosUV* input [[buffer(0)]],
							 uint id [[vertex_id]]) {
	FSIn res;
	res.pos = float4(input[id].pos, 0.f, 1.f);
	res.uv = input[id].uv;
	return res;
}

#define MAX_LIGHTS 2
struct cScene {
	float4x4 ip;
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
	float n, f;
};
struct DeferredOut {
	float4 frag [[color(0)]];
	float4 debug [[color(1)]];
};

fragment DeferredOut deferred_fs_main(FSIn input [[stage_in]],
							  constant cScene& scene [[buffer(0)]],
							  texture2d<float> color [[texture(0)]],
							  texture2d<float> normal [[texture(1)]],
							  texture2d<float> material [[texture(2)]],
							  texture2d<float> debug [[texture(3)]],
							  texture2d<float> depth [[texture(4)]],
							  sampler smp [[sampler(0)]]) {
	float3 diffuseColor = color.sample(smp, input.uv).rgb;
	float3 n = Decode(normal.sample(smp, input.uv).xy);
	float4 mat = material.sample(smp, input.uv);
	float3 world_pos = WorldPosFormDepth(input.uv, scene.ip, depth.sample(smp, input.uv).x);

	float3 diff = float3(0.f, 0.f, 0.f);
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		diff += ComputePointLight_Diffuse(scene.light[i],
										world_pos,
										n,
										diffuseColor);
		diff += (float3(1.f) - diff) * ComputePointLight_Phong(scene.light[i],
										scene.eyePos,
										world_pos,
										n,
										diffuseColor,
										mat.x/*specular*/,
										mat.y/*power*/);
	}
	DeferredOut res;
	res.frag = float4(diff, 1.f);
	res.debug = float4(n/*world_pos*/, 1.f);
	return res;
}
