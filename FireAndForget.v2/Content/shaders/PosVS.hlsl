#include "ShaderStructs.hlsli"
#include "ShaderInput.hlsli"
ConstantBuffer<Object> obj : register(b0);

PS_PN main(VS_PN input) {
	PS_PN output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, obj.mvp);
	output.nWS = normalize(mul(input.n, obj.m));
	output.nVS = normalize(mul(input.n, obj.mv));
	output.p = mul(pos, obj.m);
	return output;
}