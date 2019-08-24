#include "ShaderInput.hlsli"

PS_UV main(VS_P2UV input) {
	PS_UV res;
	res.pos = float4(input.pos, 0.f, 1.f);
	res.uv = input.uv;
	return res;
}