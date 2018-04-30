#include "Common_include.metal"
using namespace metal;
// as seen on https://aras-p.info/texts/CompactNormalStorage.html
// Method #4

float2 Encode(float3 v) {
	float p = sqrt(v.z * 8.f + 8.f);
	return float2(v.xy/p + .5f);
}

float3 Decode(float2 v) {
	float2 fenc = v * 4.f - 2.f;
	float f = dot(fenc,fenc);
	float g = sqrt(1.f - f / 4.f);
	float3 n;
	n.xy = fenc*g;
	n.z = 1.f - f/2.f;
	return n;
}

float LinearizeDepth(float depth, float n, float f) {
	return (2.0 * n) / (f + n - depth * (f - n));
}
