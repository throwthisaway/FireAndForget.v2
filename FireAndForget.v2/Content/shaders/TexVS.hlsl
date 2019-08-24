#include "ShaderInput.hlsli"
#include "ShaderStructs.h"
ConstantBuffer<Object> obj : register(b0);

PS_UVN main(VS_PNUV input) {
	PS_UVN output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(obj.mvp, pos);
	output.n = normalize(mul(obj.m, input.n));
	output.uv = input.uv;
	return output;
}