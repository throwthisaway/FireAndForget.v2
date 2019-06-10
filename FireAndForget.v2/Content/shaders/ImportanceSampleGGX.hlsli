#include "PI.hlsli"
float3 ImportanceSampleGGX(float2 xi, float3x3 mat/*spherical to cartesian*/, const float a2minus1/*a*a-1*/) {
	float phi = 2.f * M_PI_F * xi.x;
	float cosTheta = sqrt((1.f - xi.y) / (1.f + a2minus1 * xi.y));	// TODO:: why???
	float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	float3 h = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	return normalize(mul(h, mat));
}