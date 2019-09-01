#include "ShaderInput.hlsli"

cbuffer cIP : register(b0) {
	float4x4 ip;
};
struct PUV {
	float2 pos, uv;
};
static const float2 uv[] = { { 0.f, 1.f }, { 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 0.f } };

PS_PUV main(uint id : SV_VertexID) {
	PS_PUV res;
	float2 pos = uv[id] * 2.f - 1.f;
	res.pos = float4(pos.x, -pos.y, 0.f, 1.f);
	// https://github.com/d3dcoder/d3d12book/blob/master/Chapter%2021%20Ambient%20Occlusion/Ssao/Shaders/Ssao.hlsl
	// to view space near-plane
	float4 p = mul(ip, res.pos);
	res.p = p.xyz / p.w;
	res.uv = uv[id];
	return res;
}