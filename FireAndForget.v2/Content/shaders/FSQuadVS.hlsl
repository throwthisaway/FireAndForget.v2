#include "ShaderInput.hlsli"

struct PUV {
	float2 pos, uv;
};
static const PUV quad[] = { { { -1., -1.f }, { 0.f, 1.f } }, { { -1., 1.f },{ 0.f, 0.f } }, { { 1., -1.f },{ 1.f, 1.f } }, { { 1., 1.f },{ 1.f, 0.f } } };;

PS_UV main(uint id : SV_VertexID) {
	PS_UV res;
	res.pos = float4(quad[id].pos, 0.f, 1.f);
	res.uv = quad[id].uv;
	return res;
}