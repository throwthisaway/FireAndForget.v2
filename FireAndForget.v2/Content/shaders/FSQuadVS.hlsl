#include "VertexTypes.hlsli"
#include "PSInput.hlsli"

PS_T main(VertexFSQuad input) {
	PS_T res;
	res.pos = float4(input.pos, 0.f, 1.f);
	res.uv = input.uv;
	return res;
}