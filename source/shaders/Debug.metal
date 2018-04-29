#include <metal_stdlib>
#include "Common_include.metal"
vertex float4 debug_vs_main(float3 pos [[attribute(0)]][[stage_in]],
						  constant float4x4& mvp [[buffer(1)]]) {
	return float4(pos, 1.f) * mvp;
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
