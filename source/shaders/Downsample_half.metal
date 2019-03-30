#include <metal_stdlib>
#include "VertexTypes.h.metal"
#include "FragInput.h.metal"
using namespace metal;

fragment float downsample_fs_main(FragT input [[stage_in]],
								constant uint2& dim,
								texture2d<float, access::read> tex) {
	return tex.read(uint2(input.uv * float2(dim))).r;
}

