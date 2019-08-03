#include "Common.h.metal"
#include "VertexTypes.h.metal"
#include "../cpp/ShaderStructs.h"

// TODO:: declare sampler and texture
struct uObject {
	float4x4 mvp;
	float4x4 m;
};

struct FSIn {
	float4 pos [[position]];
	float3 n [[user(normal)]];
	float2 uv0 [[user(texcoord0)]];
};
vertex FSIn modo_tex_vs_main(const device VertexPNT* input [[buffer(0)]],
						constant uObject& obj [[buffer(1)]],
						uint id [[vertex_id]]) {
	FSIn output;
	float4 pos = float4(input[id].pos, 1.f);
	output.pos = pos * obj.mvp;
	output.n = float3(input[id].n) * float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz);
	output.uv0 = input[id].uv0;

	return output;
}
fragment FragOut modo_tex_fs_main(FSIn input [[stage_in]],
							 array<texture2d<float>, 4> textures [[texture(0)]],
							 sampler smp [[sampler(0)]]) {
	FragOut output;
	output.albedo = textures[0].sample(smp, input.uv0);
	output.normal = Encode(normalize(input.n + textures[1].sample(smp, input.uv0).xyz));
	float metallic = textures[2].sample(smp, input.uv0).x;
	float roughness = textures[3].sample(smp, input.uv0).x;
	output.material = float4(metallic, roughness, 0.f, 1.f);
	output.debug = input.pos;
	return output;
}
