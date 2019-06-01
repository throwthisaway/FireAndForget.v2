#include "CubeEnvMapRS.hlsli"
#include "PSInput.hlsli"
#include "PI.hlsli"

TextureCube<float4> envMap : register(t0);
SamplerState smp : register( s0 );

[RootSignature(CubeEnvMapRS)]
float4 main(PS_P input) : SV_TARGET {
	float3 up = float3(0.f, 1.f, 0.f);
	float3 n = normalize(input.p); // TODO:: why negate?
	float3 right = cross(up, n);
	up = cross(n, right);
	float3x3 mat = float3x3(right, up, n);
	float count = 0.f;
	float3 irradiance = 0.f;
	const float d = .025f;
	for (float phi = 0.f; phi < 2.f * M_PI_F; phi += d) {
		float cosPhi = cos(phi);
		float sinPhi = sqrt(1.f - cosPhi * cosPhi);
		for (float theta = 0.f; theta < .5f * M_PI_F; theta += d) {
			float cosTheta = cos(theta);
			float sinTheta = sqrt(1.f - cosTheta * cosTheta);
			float3 spherical = float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);	// in tangent space
			float3 cartesian = mul(spherical, mat);
			irradiance += envMap.Sample(smp, cartesian).rgb * cosTheta * sinTheta;
			++count;
		}
	}
	// diffuse = kD * albedoColor / PI -> hence we can spare the division by PI later
	irradiance = (M_PI_F * irradiance) / count;
	return float4(irradiance, 1.f);
}