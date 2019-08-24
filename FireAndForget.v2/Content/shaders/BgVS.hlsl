#include "ShaderInput.hlsli"
#include "BgRS.hlsli"

cbuffer cb : register(b0) {
	float4x4 vp;
};

[RootSignature(BgRS)]
PS_P main(VS_PN input) {
	PS_P output;
	float4 pos = mul(vp, float4(input.pos, 0.f/*ignore translation*/));
	output.pos = pos.xyww;	// render at depth 1.
	output.p = input.pos;
	return output;
}