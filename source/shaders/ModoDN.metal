#include "Common.h.metal"
#include "VertexTypes.h.metal"
#include "ShaderStructs.h.metal"
#include "FragInput.h.metal"

vertex FS_UVNT modo_dn_vs_main(const device VertexPNTUV* input [[buffer(0)]],
						constant Object& obj [[buffer(1)]],
						uint id [[vertex_id]]) {
	FS_UVNT output;
	float4 pos = float4(input[id].pos, 1.f);
	output.pos = obj.mvp * pos;
	output.nWS = float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz) * float3(input[id].n);
	output.nVS = float3x3(obj.mv[0].xyz, obj.mv[1].xyz, obj.mv[2].xyz) * float3(input[id].n);
	output.tWS = float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz) * float3(input[id].t);
	output.uv = input[id].uv;
	return output;
}
fragment MRTOut modo_dn_fs_main(FS_UVNT input [[stage_in]],
								   array<texture2d<float>, 2> textures [[texture(0)]],
								   sampler smp [[sampler(0)]],
								   constant Material& material [[buffer(0)]]) {
	MRTOut output;
	output.albedo = textures[0].sample(smp, input.uv);
	float3 nTx = textures[1].sample(smp, input.uv).xyz * 2.f - 1.f;
	output.normalVS = float4(normalize(input.nVS), 0.f);
	float3 nWS = normalize(input.nWS);
	float3 t = normalize(input.tWS - dot(input.tWS, nWS)*nWS);
	float3 b = cross(nWS, t);
	float3x3 tbn = float3x3(t, b, nWS);
	nWS = tbn * nTx;
	output.normalWS = float4(nWS, 0.f);
	output.material = float4(material.metallic_roughness, 0.f, 1.f);
	return output;
}
