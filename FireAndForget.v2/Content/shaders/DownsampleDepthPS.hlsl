#include "DownsampleRS.hlsli"
#include "ShaderInput.hlsli"

cbuffer cb : register(b0) {
	float factor;
};
Texture2D<float4> tex : register(t0);

[RootSignature(DownsampleRS)]
float main(PS_UV input) : SV_TARGET {
	return tex.Load(uint3(input.pos.xy * factor, 0)).r;
}