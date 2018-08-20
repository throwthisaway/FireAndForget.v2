#include "Common.hlsli"
#include "../../../source/cpp/ShaderStructs.h"

cbuffer cMaterial : register(b0) {
	Material mat;
};

struct PSIn {
	float4 pos : SV_POSITION;
	float4 worldPos : POSITION0;
	float3 n : NORMAL0;
};
MRTOut main(PSIn input) {
	MRTOut output;
	output.albedo = float4(mat.diffuse, 1.f);
	output.normal = Encode(normalize(input.n));
	output.material = float4(mat.specular, mat.power, 0.f, 1.f);
	output.debug = input.worldPos;
	return output;
}