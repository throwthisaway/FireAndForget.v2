#include "Common.h.metal"
#include "VertexTypes.h.metal"
#include "ShaderStructs.h.metal"
#include "FragInput.h.metal"

vertex FS_PN pos_vs_main(const device VertexPN* input[[buffer(0)]],
						// TODO:: apparently the count depends on how the vertex buffers attached, 1 for vb
						constant Object& obj [[buffer(1)]],
						uint id [[vertex_id]]) {
	FS_PN output;
	float4 pos = float4(input[id].pos, 1.f);
	output.pos = obj.mvp * pos;
	output.pWS = obj.m * pos;
	output.nWS = float3(obj.m * float4(input[id].n, 0.f));// TODO:: float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz);
	output.nVS = float3(obj.mv * float4(input[id].n, 0.f));
	return output;
}

fragment MRTOut pos_fs_main(FS_PN input [[stage_in]],
							 constant Material& material [[buffer(0)]]) {
	MRTOut output;
	output.albedo = float4(material.diffuse, 1.f);
	output.normalWS = float4(normalize(input.nWS), 0.f);
	output.normalVS = float4(normalize(input.nVS), 0.f);
	output.material = float4(material.metallic_roughness, 0.f, 0.f);
	//output.debug = float4(float3(input.worldPos), 1.f);
	//output.debug = float4(normalize(input.n), 1.f);
	return output;
}

