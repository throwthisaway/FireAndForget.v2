#include "VertexTypes.hlsli"
#include "PSInput.hlsli"
#include "ShaderStructs.h"

ConstantBuffer<Object> obj : register(b0);

PS_TN_TBN main(VertexPNT input, uint id : SV_VertexID) {
	PS_TN_TBN output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(obj.mvp, pos);
	float3 n = normalize(mul(obj.m, input.n));
	float3 p1 = normalize(input.pos[id + 1] - input.pos);
	float3 up = float3(0.f, 1.f, 0.f);
	if (n.y > .999f) up = float3(0.f, 0.f, 1.f);
	float3 t = normalize(cross(n, up)), b = normalize(cross(t, n));
	output.tbn = float3x3(t, b, n);
	output.n = n;
	output.uv = input.uv;
	return output;
}