#include "Phong_include.metal"
#include "Common_include.metal"
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
#define MAX_LIGHTS 2
struct cMaterial {
	Material mat;
};
struct cScene {
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
};

fragment FragOut tex_fs_main(FSIn input [[stage_in]],
							texture2d<float> diffuseTexture [[texture(0)]],
							sampler smp [[sampler(0)]],
							constant cMaterial& material [[buffer(0)]],
							constant cScene& scene [[buffer(1)]]) {
	float3 diffuseColor = diffuseTexture.sample(smp, input.uv0).rgb;
	float3 diff = float3(0.f, 0.f, 0.f);
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		diff += ComputePointLight_Diffuse(scene.light[i],
										  float3(input.world_pos),
										  input.n,
										  diffuseColor);
	}
	FragOut output;
	output.albedo = float4(diff, 1.f);
	output.normal = Encode(input.n);
	output.material = float4(material.mat.specular, material.mat.power, 0.f, 1.f);
	return output;
}
