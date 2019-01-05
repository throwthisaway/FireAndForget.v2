#include "VertexTypes.h.metal"
using namespace metal;

struct FSIn {
	float4 pos [[position]];
	float3 p;
};

vertex FSIn bg_vs_main(const device VertexPN* input [[buffer(0)]],
					  constant float4x4& vp [[buffer(1)]],
					  uint id [[vertex_id]]) {

	float4 pos = vp * float4(input[id].pos, 0.f/*ignore translation*/);
	FSIn output;
	output.pos = pos.xyww;	// render at depth 1.
	output.p = float3(pos);
	return output;
}

constant float gamma = 2.2f, invExp = 1.f/gamma;
fragment float4 bg_fs_main(FSIn input [[stage_in]],
						   texturecube<float> envMap [[texture(0)]],
						   sampler smp [[sampler(0)]]) {
	float3 c = envMap.sample(smp, input.p).rgb;
	c = c / (c + float3(1.f)); // tone-map
	c = pow(c, invExp);
	return float4(c, 1.f);
}
