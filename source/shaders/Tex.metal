#include "Common.h.metal"
#include "VertexTypes.h.metal"
#include "ShaderStructs.h.metal"
#include "Fraginput.h.metal"

vertex FS_UVN tex_vs_main(const device VertexPNUV* input [[buffer(0)]],
						constant Object& obj [[buffer(1)]],
						uint id [[vertex_id]]) {
	FS_UVN output;
	float4 pos = float4(input[id].pos, 1.f);
	output.pos = obj.mvp * pos;
	output.nWS = float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz) * float3(input[id].n);
	output.nVS = float3x3(obj.mv[0].xyz, obj.mv[1].xyz, obj.mv[2].xyz) * float3(input[id].n);
	output.uv = input[id].uv;
	return output;
}
fragment MRTOut tex_fs_main(FS_UVN input [[stage_in]],
							texture2d<float> diffuseTexture [[texture(0)]],
							sampler smp [[sampler(0)]],
							constant Material& material [[buffer(0)]]) {
	MRTOut output;
	output.albedo = diffuseTexture.sample(smp, input.uv);;
	output.normalWS = float4(normalize(input.nWS), 0.f);
	output.normalVS = float4(normalize(input.nVS), 0.f);
	output.material = float4(material.metallic_roughness, 0.f, 0.f);
	return output;
}
