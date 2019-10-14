#include "ShaderInput.hlsli"
#include "ShaderStructs.hlsli"

ConstantBuffer<Object> obj : register(b0);
GS_PUVN main(VS_PNUV input) {
	GS_PUVN output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, obj.mvp);
	output.nWS = normalize(mul(input.n, obj.m));
	output.nVS = normalize(mul(input.n, obj.mv));
	output.p = mul(input.pos, obj.m);
	output.uv = input.uv;
	return output;
}
