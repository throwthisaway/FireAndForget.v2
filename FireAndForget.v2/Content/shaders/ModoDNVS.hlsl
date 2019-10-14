#include "ShaderInput.hlsli"
#include "ShaderStructs.hlsli"

ConstantBuffer<Object> obj : register(b0);
PS_UVNT main(VS_PNTUV input) {
	PS_UVNT output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, obj.mvp);
	output.nWS = normalize(mul(input.n, obj.m));
	output.nVS = normalize(mul(input.n, obj.mv));
	output.tWS = mul(input.t, obj.m);
	output.uv = input.uv;
	return output;
}
