#include "CubeEnvMapRS.hlsli"
#include "PSInput.hlsli"
#include "PI.hlsli"

Texture2D<float4> envMap : register(t0);
SamplerState smp : register( s0 );

[RootSignature(CubeEnvMapRS)]
float4 main(PS_P input) : SV_TARGET {
	const float2 inv = -float2(.5f / M_PI_F, 1.f / M_PI_F); /* -1 turns it upside down */
	float3 p = normalize(input.p);
	float2 uv = float2(atan2(p.z, p.x), asin(p.y)) * inv + .5f;
	return envMap.Sample(smp, uv);
}