#include "Common.hlsli"
#include "ShaderStructs.h"

ConstantBuffer<Material> mat : register(b0);

struct PSIn {
	float4 pos : SV_POSITION;
	float4 worldPos : POSITION0;
	float3 n : NORMAL0;
};
MRTOut main(PSIn input) {
	MRTOut output;
	output.albedo = float4(mat.diffuse, 1.f);
	float3 n = normalize(input.n);
	output.normal = float4(n, 1.f);
	output.material = float4(mat.metallic_roughness, 0.f, 1.f);
	output.debug = input.pos;
	return output;
}