#include "Common.h.metal"
#include "VertexTypes.h.metal"
#include "ShaderStructs.h.metal"
#include "FragInput.h.metal"

fragment MRTOut modo_dnmr_fs_main(FS_UVNT input [[stage_in]],
							 array<texture2d<float>, 4> textures [[texture(0)]],
							 sampler smp [[sampler(0)]]) {
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
	float metallic = textures[2].sample(smp, input.uv).x;
	float roughness = textures[3].sample(smp, input.uv).x;
	output.material = float4(metallic, roughness, 0.f, 1.f);
	return output;
}
