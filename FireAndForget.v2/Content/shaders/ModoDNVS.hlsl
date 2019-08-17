#include "VertexTypes.hlsli"
#include "PSInput.hlsli"
#include "ShaderStructs.h"

ConstantBuffer<Object> obj : register(b0);

PS_TN_TNB main(VertexPNT input, uint id : SV_VertexID) {
	PS_TN_TNB output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(obj.mvp, pos);
	float3 p1 = normalize(input.pos[id + 1] - input.pos);
	float3 t = normalize(mul(input.n, p1)), b = normalize(cross(input.n, t));
	output.tnb = float3x3(t, input.n, b);
	output.n = normalize(mul(obj.m, input.n));
	output.uv = input.uv;
	return output;
}