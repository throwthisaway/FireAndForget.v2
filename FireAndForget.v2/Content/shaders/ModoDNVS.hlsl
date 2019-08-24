#include "ShaderInput.hlsli"
#include "ShaderStructs.h"

ConstantBuffer<Object> obj : register(b0);

GS_UVN main(VS_PNUV input, uint id : SV_VertexID) {
	GS_UVN output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(obj.mvp, pos);
	output.n = normalize(mul(obj.m, input.n));
	output.uv = input.uv;
	return output;
}