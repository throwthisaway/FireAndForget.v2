#include "ShaderInput.hlsli"
#include "ShaderStructs.hlsli"
ConstantBuffer<Object> obj : register(b0);

PS_PUVN main(VS_PNUV input) {
	PS_PUVN output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, obj.mvp);
	output.nWS = normalize(mul(input.n, obj.m));
	output.nVS = normalize(mul(input.n, obj.mv));
	output.uv = input.uv;
	output.pWS = mul(pos, obj.m).xyz;
	return output;
}