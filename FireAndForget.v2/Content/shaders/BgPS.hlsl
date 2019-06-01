#include "PSInput.hlsli"
#include "BgRS.hlsli"

TextureCube<float4> texCube : register(t0);
SamplerState smp : register(s0);

[RootSignature(BgRS)]
float4 main(PS_P input) : SV_TARGET {
	const float gamma = 2.2f, invExp = 1.f/gamma;
	float3 c = texCube.Sample(smp, input.p).rgb;
	c = c / (c + float3(1.f)); // tone-map
	c = pow(c, invExp);
	return float4(c, 1.f);
}