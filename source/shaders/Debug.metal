#include <metal_stdlib>
#include "Common.h.metal"
#include "VertexTypes.h.metal"

vertex float4 debug_vs_main(const device VertexPN* input [[buffer(0)]],
							constant float4x4& mvp [[buffer(1)]],
							uint vid [[vertex_id]]) {
	return float4(input[vid].pos, 1.f) * mvp;
}

fragment FragOut debug_fs_main(float4 vert_in [[stage_in]],
							constant float4& col [[buffer(0)]]) {
	FragOut output;
	output.albedo = col;
	output.normal = normalize(float2(1.f, 1.f));
	output.material = float4(0.f, 0.f, 0.f, 1.f);
	output.debug = float4(0.f);//vert_in;
	return output;
}
