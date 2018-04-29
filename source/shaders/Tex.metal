#include "Common_include.metal"
#include "ShaderStructs.metal"

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
	float3 n [[user(normal)]];
	float2 uv0;
};
vertex FSIn tex_vs_main(VIn input [[stage_in]],
						constant uObject& obj [[buffer(3)]],
						uint id [[vertex_id]]) {
	FSIn output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = pos * obj.mvp;
	output.n = input.n * float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz);
	output.uv0 = input.uv0;

	return output;
}
struct cMaterial {
	Material mat;
};
fragment FragOut tex_fs_main(FSIn input [[stage_in]],
							texture2d<float> diffuseTexture [[texture(0)]],
							sampler smp [[sampler(0)]],
							constant cMaterial& material [[buffer(0)]]) {
	float4 diffuseColor = diffuseTexture.sample(smp, input.uv0);
	FragOut output;
	output.albedo = diffuseColor;
	output.normal = Encode(input.n);
	output.material = float4(material.mat.specular, material.mat.power, 0.f, 1.f);
	output.debug = input.pos;
	return output;
}
