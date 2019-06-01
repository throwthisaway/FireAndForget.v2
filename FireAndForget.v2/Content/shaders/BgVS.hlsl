#include "VertexTypes.hlsli"
#include "PSInput.hlsli"
#include "BgRS.hlsli"

ConstantBuffer<float4x4> vp : register(b0);

[RootSignature(BgRS)]
PS_P main(VertexPN input) {
	PS_P output;
	float4 pos = mul(float4(input[id].pos, 0.f/*ignore translation*/), vp);
	output.pos = pos.xyww;	// render at depth 1.
	output.p = input.pos;
	return output;
}