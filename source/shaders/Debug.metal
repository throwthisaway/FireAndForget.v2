#include <metal_stdlib>
using namespace metal;

vertex float4 debug_vs_main(float3 pos [[attribute(0)]][[stage_in]],
						  constant float4x4& mvp [[buffer(1)]]) {
	return float4(pos, 1.f) * mvp;
}

fragment float4 debug_fs_main(float4 vert_in [[stage_in]],
							constant float4& col [[buffer(0)]]) {
	return col;
}
