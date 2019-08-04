#include "VertexTypes.hlsli"
#include "PSInput.hlsli"
#include "../../../source/cpp/ShaderStructs.h"
ConstantBuffer<Object> obj : register(b0);

PS_TN main(VertexPNT input) {
	PS_TN output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(obj.mvp, pos);
	output.n = normalize(mul(obj.m, input.n));
	output.uv = input.uv;
	return output;
}