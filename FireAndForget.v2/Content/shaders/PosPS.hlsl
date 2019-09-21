#include "Common.hlsli"
#include "ShaderStructs.h"
#include "ShaderInput.hlsli"

ConstantBuffer<Material> mat : register(b0);

MRTOut main(PS_PN input) {
	MRTOut output;
	output.albedo = float4(mat.diffuse, 1.f);
	float3 nWS = normalize(input.nWS);
	output.normalWS = float4(nWS, 1.f);
	output.normalVS = float4(normalize(input.nVS), 1.f);
	output.material = float4(mat.metallic_roughness, 0.f, 1.f);
	output.debug = input.pos;
	return output;
}