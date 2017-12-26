#include <metal_stdlib>
using namespace metal;

// TODO:: declare sampler and texture
struct uObject {
	float4x4 mvp;
	float4x4 m;
};
struct VIn {
	float3 pos;
	float3 n;
	float2 uv0;
};

struct FSIn {
	float4 pos [[position]];
	float4 world_pos;
	float3 n;
	float2 uv0;
};
vertex FSIn tex_vs_main(constant VIn* input [[buffer(0)]],
						constant uObject& obj [[buffer(1)]],
						uint id [[vertex_id]]) {
	FSIn output;
	float4 pos = float4(input[id].pos, 1.f);
	output.pos = obj.mvp * pos;
	output.world_pos = obj.m * pos;
	output.n = normalize(float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz) * input->n);
	output.uv0 = input->uv0;

	return output;
}

fragment float4 tex_fs_main(FSIn input [[stage_in]]) {
	// TODO::
	return float4(1.f, .5f, .5f, 1.f);
}
