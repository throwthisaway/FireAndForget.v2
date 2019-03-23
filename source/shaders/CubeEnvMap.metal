#include <metal_stdlib>
#include "VertexTypes.h.metal"
#include "FragInput.h.metal"

vertex FragP cubeenv_vs_main(const device VertexPN* input [[buffer(0)]],
						constant float4x4& mvp [[buffer(1)]],
						uint id [[vertex_id]]) {
	FragP output;
	output.pos = mvp * float4(input[id].pos, 1.f);
	output.p = float3(input[id].pos);
	return output;
}
constant float2 inv = -float2(.5f / M_PI_F, 1.f / M_PI_F); /* -1 turns it upside down */
fragment float4 cubeenv_fs_main(FragP input [[stage_in]],
								texture2d<float> diffuseTexture [[texture(0)]],
								sampler smp [[sampler(0)]]) {

	float3 p = normalize(input.p);
	float2 uv = float2(atan2(p.z, p.x), asin(p.y)) * inv + .5f;
	return diffuseTexture.sample(smp, uv);
}

fragment float4 cubeir_fs_main(FragP input [[stage_in]],
							   texturecube<float> envMap [[texture(0)]],
							   sampler smp [[sampler(0)]]) {
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
			float3 cartesian = mat * spherical;
			irradiance += envMap.sample(smp, cartesian).rgb * cosTheta * sinTheta;
			++count;
		}
	}
	// diffuse = kD * albedoColor / PI -> hence we can spare the division by PI later
	irradiance = (M_PI_F * irradiance) / count;
	return float4(irradiance, 1.f);
}

// TODO:: perf analysis
//fragment float4 cubeir_fs_main(FSIn input [[stage_in]],
//							   texturecube<float> envMap [[texture(0)]],
//							   sampler smp [[sampler(0)]]) {
//	float3 up = float3(0.f, 1.f, 0.f);
//	float3 n = normalize(input.p);
//	float3 right = cross(up, n);
//	up = cross(n, right);
//	float3x3 mat = float3x3(right, up, n);
//	float count = 0.f;
//	float3 irradiance = 0.f;
//	const float d = .025f;
//	for (float phi = 0.f; phi < 2.f * M_PI_F; phi += d)
//		for (float theta = 0.f; theta < .5f * M_PI_F; theta += d) {
//			float3 spherical = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));	// in tangent space
//			float3 cartesian = mat * spherical;
//			irradiance += envMap.sample(smp, cartesian).rgb * cos(theta) * sin(theta);
//			++count;
//		}
//	// diffuse = kD * albedoColor / PI -> hence we can spare the division by PI later
//	irradiance = (M_PI_F * irradiance) / count;
//	return float4(irradiance, 1.f);
//}

