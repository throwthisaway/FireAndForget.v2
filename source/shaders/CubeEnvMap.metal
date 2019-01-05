#include <metal_stdlib>
#include "VertexTypes.h.metal"
using namespace metal;

struct FSIn {
	float4 pos [[position]];
	float3 p;
};
vertex FSIn cubeenv_vs_main(const device VertexPN* input [[buffer(0)]],
						constant float4x4& mvp [[buffer(1)]],
						uint id [[vertex_id]]) {
	FSIn output;
	output.pos = float4(input[id].pos, 1.f) * mvp;
	output.p = float3(input[id].pos);
	return output;
}
constant float2 inv = float2(.5f / M_PI_F, 1.f / M_PI_F);
fragment float4 cubeenv_fs_main(FSIn input [[stage_in]],
								texture2d<float> diffuseTexture [[texture(0)]],
								sampler smp [[sampler(0)]]) {

	float3 p = normalize(input.p);
	float2 uv = float2(atan2(p.z, p.x), asin(p.y)) * inv + .5f;
	return diffuseTexture.sample(smp, uv);
}
