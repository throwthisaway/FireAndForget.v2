#include <metal_stdlib>
#include "FragInput.h.metal"

using namespace metal;

constant float2 uv[] = { { 0.f, 1.f }, { 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 0.f } };

vertex FS_UV fsquad_vs_main(uint id [[vertex_id]]) {
	FS_UV res;
	float2 pos = uv[id] * 2.f - 1.f;
	res.pos = float4(pos.x, -pos.y, 0.f, 1.f);
	res.uv = uv[id];
	return res;
}
