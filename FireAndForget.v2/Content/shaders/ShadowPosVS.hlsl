#include "ShaderInput.hlsli"
cbuffer cBuffer : register(b0) {
	row_major float4x4 mvp;
};
float4 main(VS_PN input) : SV_POSITION {
	float4 pos = float4(input.pos, 1.f);
	pos = mul(pos, mvp);
	return pos;
}