#include <metal_stdlib>
#include "FragInput.h.metal"
using namespace metal;

fragment float blur4x4r32_fs_main(FS_UV input [[stage_in]],
		   texture2d<float> tex [[texture(0)]],
		   sampler smp [[sampler(0)]]) {
	float2 texelSize = 1.f / float2(tex.get_width(), tex.get_height()), halfTexelSize = texelSize * .5f, doubleTexelSize = texelSize * 2.f;
	float texel0 = tex.sample(smp, input.uv + halfTexelSize).x,
		texel1 = tex.sample(smp, input.uv + float2(doubleTexelSize.x, 0.f) + halfTexelSize).x,
		texel2 = tex.sample(smp, input.uv + float2(0.f, doubleTexelSize.y) + halfTexelSize).x,
		texel3 = tex.sample(smp, input.uv + doubleTexelSize + halfTexelSize).x;
	float result = (texel0 + texel1 + texel2 + texel3) * .25f;
	return result;
}
