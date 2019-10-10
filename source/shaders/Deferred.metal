#include <metal_stdlib>
#include "Common.h.metal"
#include "Phong.h.metal"
#include "VertexTypes.h.metal"
#include "FragInput.h.metal"

vertex FS_UV fsquad_vs_main(constant VertexFSQuad* input [[buffer(0)]],
							 uint id [[vertex_id]]) {
	FS_UV res;
	res.pos = float4(input[id].pos, 0.f, 1.f);
	res.uv = input[id].uv;
	return res;
}

struct DeferredOut {
	float4 frag [[color(0)]];
	float4 debug [[color(1)]];
};

fragment DeferredOut deferred_fs_main(FS_UV input [[stage_in]],
							  constant SceneCB& scene [[buffer(0)]],
							  texture2d<float> color [[texture(0)]],
							  texture2d<float> normal [[texture(1)]],
							  texture2d<float> material [[texture(2)]],
							  texture2d<float> debug [[texture(3)]],
							  texture2d<float> depth [[texture(4)]],
							  sampler smp [[sampler(0)]]) {
	float3 diffuseColor = color.sample(smp, input.uv).rgb;
	float3 n = normal.sample(smp, input.uv).xyz;
	float4 mat = material.sample(smp, input.uv);
	float3 world_pos = WorldPosFormDepth(input.uv, scene.ip, depth.sample(smp, input.uv).x);

	float3 diff = float3(0.f, 0.f, 0.f);
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		PointLight pl = scene.light[i];
		diff += ComputePointLight_Diffuse(pl,
										world_pos,
										n,
										diffuseColor);
		diff += ComputePointLight_Phong(pl,
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
