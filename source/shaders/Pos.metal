#include "Common_include.metal"
#include "ShaderStructs.metal"

struct cObject {
	float4x4 mvp, m;
};
struct VIn {
	float3 pos [[attribute(0)]];
	float3 n [[attribute(1)]];
};
struct PSIn {
	float4 pos [[position]];
	float3 n [[user(normal)]];
};

vertex PSIn pos_vs_main(VIn input[[stage_in]],
						// TODO:: apparently the count depends on how the vertex buffers attached, 1 for vb, 1 for nb
						constant cObject& obj [[buffer(2)]]) {
	PSIn output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = pos * obj.mvp;
	output.n = input.n * float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz);
	return output;
}
struct cMaterial {
	Material mat;
};
fragment FragOut pos_fs_main(PSIn input [[stage_in]],
							 constant cMaterial& material [[buffer(0)]]) {
	FragOut output;
	output.albedo = float4(material.mat.diffuse, 1.f);
	output.normal = Encode(input.n);
	output.material = float4(material.mat.specular, material.mat.power, 0.f, 1.f);
	output.debug = input.pos;
	return output;
}

