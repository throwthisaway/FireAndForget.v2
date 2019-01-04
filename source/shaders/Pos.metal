#include "Common.h.metal"
#include "VertexTypes.h.metal"
#include "../cpp/ShaderStructs.h"

struct PSIn {
	float4 pos [[position]];
	float3 n [[user(normal)]];
	float4 worldPos [[user(position)]];
	float2 depthCS;
};

vertex PSIn pos_vs_main(const device VPN* input[[buffer(0)]],
						// TODO:: apparently the count depends on how the vertex buffers attached, 1 for vb, 1 for nb
						constant Object& obj [[buffer(1)]],
						uint id [[vertex_id]]) {
	PSIn output;
	float4 pos = float4(input[id].pos, 1.f);
	output.pos = pos * obj.mvp;
	output.worldPos = pos * obj.m;
	output.n = float3(float4(input[id].n, 0.f) * obj.m);// TODO:: float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz);
	output.depthCS = float2(output.pos.z, output.pos.w);
	return output;
}

fragment FragOut pos_fs_main(PSIn input [[stage_in]],
							 constant Material& material [[buffer(0)]]) {
	FragOut output;
	output.albedo = float4(material.diffuse, 1.f);
	output.normal = Encode(normalize(input.n));
	output.material = float4(material.specular_power, 0.f, 1.f);
	output.debug = input.worldPos;
	return output;
}

