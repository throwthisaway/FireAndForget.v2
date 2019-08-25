#include "ShaderInput.hlsli"
#include "Common.hlsli"
#include "BgRs.hlsli"

TextureCube<float4> texCube : register(t0);
SamplerState smp : register(s0);

[RootSignature(BgRS)]
float4 main(PS_P input) : SV_TARGET {
	float3 c = texCube.Sample(smp, input.p).rgb;
	return float4(GammaCorrection(c), 1.f);
}