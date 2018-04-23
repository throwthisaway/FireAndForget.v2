#include "Phong_include.metal"
#include "Common_include.metal"
struct cObject {
	float4x4 mvp, m;
};
struct VIn {
	float3 pos [[attribute(0)]];
	float3 n [[attribute(1)]];
};
struct PSIn {
	float4 pos [[position]];
	float4 world_pos;
	float3 n [[user(normal)]];
};
#define MAX_LIGHTS 2
struct cMaterial {
	Material mat;
};
struct cScene {
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
};
vertex PSIn pos_vs_main(VIn input[[stage_in]],
						// TODO:: apparently the count depends on how the vertex buffers attached, 1 for vb, 1 for nb
						constant cObject& obj [[buffer(2)]]) {
	PSIn output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = pos * obj.mvp;
	output.world_pos = pos * obj.m;
	output.n = input.n * float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz);
	return output;
}

fragment FragOut pos_fs_main(PSIn input [[stage_in]],
							constant cMaterial& material [[buffer(0)]],
							constant cScene& scene [[buffer(1)]]) {
	float3 diff = float3(0.f, 0.f, 0.f);
	for (int i = 0; i < MAX_LIGHTS; ++i) {
//		diff += ComputePointLight_Diffuse(scene.light[i],
//										float3(input.world_pos),
//										input.n,
//										material.mat.diffuse);
		diff += ComputePointLight_Phong(scene.light[i],
											 scene.eyePos,
											float3(input.world_pos),
											input.n,
											material.mat.diffuse,
											material.mat.specular,
											material.mat.power);
	}
	FragOut output;
	output.albedo = float4(diff, 1.f);
	output.normal = Encode(input.n);
	output.material = float4(material.mat.specular, material.mat.power, 0.f, 1.f);
	return output;
}

