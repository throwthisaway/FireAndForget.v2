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
};
fragment float4 deferred_fs_main(FSIn input [[stage_in]],
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
	float3 world_pos = float3x3(scene.ip[0].xyz, scene.ip[1].xyz, scene.ip[2].xyz) * float3(input.uv - .5, depth.sample(smp, input.uv).x);

	float3 diff = float3(0.f, 0.f, 0.f);
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		//		diff += ComputePointLight_Diffuse(scene.light[i],
		//										float3(input.world_pos),
		//										input.n,
		//										material.mat.diffuse);
		diff += ComputePointLight_Phong(scene.light[i],
										scene.eyePos,
										world_pos,
										n,
										diffuseColor,
										mat.x/*specular*/,
										mat.y/*power*/);
	}
	return float4(diff, 1.f);
}
