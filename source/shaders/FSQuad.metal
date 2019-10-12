#include <metal_stdlib>
#include "VertexTypes.h.metal"
#include "FragInput.h.metal"

using namespace metal;

vertex FS_UV fsquad_vs_main(constant VertexFSQuad* input [[buffer(0)]],
							 uint id [[vertex_id]]) {
	FS_UV res;
	res.pos = float4(input[id].pos, 0.f, 1.f);
	res.uv = input[id].uv;
	return res;
}
