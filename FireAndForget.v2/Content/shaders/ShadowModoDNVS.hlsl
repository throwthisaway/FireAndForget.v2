#include "ShaderInput.hlsli"
cbuffer cBuffer : register(b0) {
	float4x4 mvp;
};
float4 main(VS_PNTUV input) : SV_POSITION {
	float4 pos = float4(input.pos, 1.f);
	pos = mul(pos, mvp);
	return pos;
}