#include "CubeEnvMapRS.hlsli"
#include "ShaderInput.hlsli"

cbuffer cb : register(b0) {
	float4x4 mvp;
};

[RootSignature(CubeEnvMapRS)]
PS_P main(VS_PN input) {
	PS_P output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(mvp, pos);
	output.p = float3(input.pos);
	return output; 
}