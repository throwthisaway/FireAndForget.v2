#include "CubeEnvMapRS.hlsli"
#include "VertexTypes.hlsli"
#include "PSInput.hlsli"

cbuffer cb : register(b0) {
	float4x4 mvp;
};

[RootSignature(CubeEnvMapRS)]
PS_P main(VertexPN input) {
	PS_P output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, mvp);
	output.p = float3(input.pos);
	return output; 
}