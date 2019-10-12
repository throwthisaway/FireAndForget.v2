#include <metal_stdlib>
#include "FragInput.h.metal"
using namespace metal;

constant float2 uv[] = { { 0.f, 1.f }, { 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 0.f } };

vertex FS_PUV fsquad_viewpos_vs_main(uint id [[vertex_id]],
							  constant float4x4& ip[[buffer(0)]]) {
	FS_PUV res;
	float2 pos = uv[id] * 2.f - 1.f;
	res.pos = float4(pos.x, -pos.y, 0.f, 1.f);
	// https://github.com/d3dcoder/d3d12book/blob/master/Chapter%2021%20Ambient%20Occlusion/Ssao/Shaders/Ssao.hlsl
	// to view space near-plane
	float4 p = ip * res.pos;
	res.p = p.xyz / p.w; // near clip plane viewpos
	res.uv = uv[id];
	return res;
}
