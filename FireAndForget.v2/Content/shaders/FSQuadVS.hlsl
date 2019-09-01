#include "ShaderInput.hlsli"

struct PUV {
	float2 pos, uv;
};
static const float2 uv[] = { { 0.f, 1.f }, { 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 0.f } };

PS_UV main(uint id : SV_VertexID) {
	PS_UV res;
	float2 pos = uv[id] * 2.f - 1.f;
	res.pos = float4(pos.x, -pos.y, 0.f, 1.f);
	res.uv = uv[id];
	return res;
}