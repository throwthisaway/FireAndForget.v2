#include <metal_stdlib>
using namespace metal;
struct PosCol {
	float4 pos [[position]];
	float4 col;
};

vertex PosCol vertex_main(constant float4* pos [[buffer(0)]],
						  constant float4* col [[buffer(1)]],
						  constant float4x4& mvp [[buffer(2)]],
						  uint id [[vertex_id]]) {
	PosCol res;
	res.pos = mvp * pos[id];
	res.col = col[id];
	return res;
}

fragment float4 fragment_main(PosCol vert_in [[stage_in]]) {
	return vert_in.col;
}
