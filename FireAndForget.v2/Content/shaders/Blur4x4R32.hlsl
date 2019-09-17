#include "ShaderInput.hlsli"
#define RS "RootFlags(DENY_VERTEX_SHADER_ROOT_ACCESS |"\
			"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
			"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
			"DENY_HULL_SHADER_ROOT_ACCESS)," \
		"DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL)," \
		"StaticSampler(s0," \
			"addressU = TEXTURE_ADDRESS_CLAMP," \
			"addressV = TEXTURE_ADDRESS_CLAMP," \
			"addressW = TEXTURE_ADDRESS_CLAMP," \
			"filter = FILTER_MIN_MAG_MIP_LINEAR," \
			"visibility = SHADER_VISIBILITY_PIXEL),"
Texture2D<float> tex : register(t0);
SamplerState smp : register(s0);

[RootSignature(RS)]
float main(PS_UV input) : SV_TARGET{
	float w, h;
	tex.GetDimensions(w, h);
	float2 texelSize = 1.f / float2(w, h), halfTexelSize = texelSize * .5f, doubleTexelSize = texelSize * 2.f;
	float texel0 = tex.Sample(smp, input.uv + halfTexelSize),
		texel1 = tex.Sample(smp, input.uv + float2(doubleTexelSize.x, 0.f) + halfTexelSize),
		texel2 = tex.Sample(smp, input.uv + float2(0.f, doubleTexelSize.y) + halfTexelSize),
		texel3 = tex.Sample(smp, input.uv + doubleTexelSize + halfTexelSize);
	float result = (texel0 + texel1 + texel2 + texel3) * .25f;
	return result;
}