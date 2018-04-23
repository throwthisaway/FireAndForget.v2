#include <metal_stdlib>
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

fragment float4 deferred_fs_main(FSIn input [[stage_in]],
							  texture2d<float> color [[texture(0)]],
							  texture2d<float> normal [[texture(1)]],
							  texture2d<float> material [[texture(2)]],
							  texture2d<float> depth [[texture(3)]],
							  sampler smp [[sampler(0)]]) {
	float3 diffuseColor = color.sample(smp, input.uv).rgb;
	return float4(diffuseColor, 1.f);
}
