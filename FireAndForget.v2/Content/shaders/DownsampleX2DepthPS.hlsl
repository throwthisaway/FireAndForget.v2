#include "Downsample.hlsli"
#include "PSInput.hlsli"

ConstantBuffer<uint2> dim;
Texture2D<float4> tex : register(t0);

[RootSignature(DownsampleRS)]
float main(PS_T input) : SV_TARGET {
	return tex.load(uint2(input.uv * float2(dim))).r;
}