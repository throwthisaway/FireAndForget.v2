#include <metal_stdlib>
using namespace metal;

// TODO:: declare sampler and texture
struct uObject {
	float4x4 mvp;
	float4x4 m;
};
struct VIn {
	float3 pos [[attribute(0)]];
	float3 n [[attribute(1)]];
	float2 uv0 [[attribute(2)]];
};

struct FSIn {
	float4 pos [[position]];
	float4 world_pos;
	float3 n [[user(normal)]];
	float2 uv0;
};
vertex FSIn tex_vs_main(VIn input [[stage_in]],
						constant uObject& obj [[buffer(3)]],
						uint id [[vertex_id]]) {
	FSIn output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = pos * obj.mvp;
	output.world_pos = pos * obj.m;
	output.n = input.n * float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz);
	output.uv0 = input.uv0;

	return output;
}

fragment float4 tex_fs_main(FSIn input [[stage_in]],
							texture2d<float> diffuseTexture [[texture(0)]],
							sampler smp [[sampler(0)]]) {
	float3 diffuseColor = diffuseTexture.sample(smp, input.uv0).rgb;
	return float4(diffuseColor, 1.f);
}
