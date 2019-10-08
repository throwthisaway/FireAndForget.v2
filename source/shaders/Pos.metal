#include "Common.h.metal"
#include "VertexTypes.h.metal"
#include "ShaderStructs.h.metal"

struct PSIn {
	float4 pos [[position]];
	float3 n [[user(normal)]];
	float4 worldPos [[user(position)]];
	float2 depthCS;
};

vertex PSIn pos_vs_main(const device VertexPN* input[[buffer(0)]],
						// TODO:: apparently the count depends on how the vertex buffers attached, 1 for vb
						constant Object& obj [[buffer(1)]],
						uint id [[vertex_id]]) {
	PSIn output;
	float4 pos = float4(input[id].pos, 1.f);
	output.pos = obj.mvp * pos;
	output.worldPos = obj.m * pos;
	output.n = float3(obj.m * float4(input[id].n, 0.f));// TODO:: float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz);
	output.depthCS = float2(output.pos.z, output.pos.w);
	return output;
}

fragment FragOut pos_fs_main(PSIn input [[stage_in]],
							 constant Material& material [[buffer(0)]]) {
	FragOut output;
	output.albedo = float4(material.diffuse, 1.f);
	output.normal = Encode(normalize(input.n));
	output.material = float4(material.metallic_roughness, 0.f, 1.f);
	//output.debug = float4(float3(input.worldPos), 1.f);
	output.debug = float4(normalize(input.n), 1.f);
	return output;
}

