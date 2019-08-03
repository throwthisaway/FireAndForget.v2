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
	float2 uv0;
};
vertex FSIn tex_vs_main(const device VertexPNT* input [[buffer(0)]],
						constant uObject& obj [[buffer(1)]],
						uint id [[vertex_id]]) {
	FSIn output;
	float4 pos = float4(input[id].pos, 1.f);
	output.pos = obj.mvp * pos;
	output.n = float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz) * float3(input[id].n);
	output.uv0 = input[id].uv0;

	return output;
}
fragment FragOut tex_fs_main(FSIn input [[stage_in]],
							texture2d<float> diffuseTexture [[texture(0)]],
							sampler smp [[sampler(0)]],
							constant GPUMaterial& material [[buffer(0)]]) {
	float4 diffuseColor = diffuseTexture.sample(smp, input.uv0);
	FragOut output;
	output.albedo = diffuseColor;
	output.normal = Encode(input.n);
	output.material = float4(material.specular_power, 0.f, 1.f);
	output.debug = input.pos;
	return output;
}
