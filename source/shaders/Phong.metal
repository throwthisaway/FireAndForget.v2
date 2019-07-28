#include "Phong.h.metal"

float3 ComputePointLight_Phong(PointLight l, float3 eyePos, float3 pos, float3 normal, float3 col, float spec, float power) {
	float3 lv = l.pos - pos;	// vector towards light
	float l_distance = length(lv);
	if (l_distance > l.att_range.w) return col * l.ambient;

	lv = lv / l_distance;			// light direction vector

	float l_intensity = dot(lv, normal);	// light intensity
	if (l_intensity > 0.) {
		float3 diffuse = l_intensity * l.diffuse * col;	// diffuse term
														// specular term
		float3 r = -reflect(lv, normal);	// light reflection driection from the surface
		float3 v = normalize(eyePos.xyz - pos);	//normalized eye direction vector from the surface
		float3 specular = l.specular.xyz * pow(saturate(dot(r, v)), power) * spec;
		//attenuation
		float att = 1.0f / dot(l.att_range.xyz, float3(1.0f, l_distance, l_distance * l_distance));
		return (diffuse + specular) * att;
	}
	return col * l.ambient;
}
float3 ComputePointLight_BlinnPhong(PointLight l, float3 eyePos, float3 pos, float3 normal, float3 col, float spec, float power) {
	float3 lv = l.pos - pos;	// vector towards light
	float l_distance = length(lv);
	if (l_distance > l.att_range.w) return col * l.ambient;

	lv = lv / l_distance;			// light direction vector
	float l_intensity = dot(lv, normal);	// light intensity
	if (l_intensity > 0.) {
		float3 diffuse = l_intensity * l.diffuse * col;	// diffuse term
		float3 v = normalize(eyePos.xyz - pos);	//normalized eye direction vector from the surface
		float3 h = normalize(lv + v);	// half-vector, *.5 instead of normalize

		float3 specular = l.specular.xyz * pow(saturate(dot(h, normal)), power) * spec;
		//attenuation
		float att = 1.0f / dot(l.att_range.xyz, float3(1.0f, l_distance, l_distance * l_distance));
		return (diffuse + specular) * att;
	}
	return col * l.ambient;
}
float3 ComputePointLight_Diffuse(PointLight l, float3 pos, float3 normal, float3 col) {
	float3 lv = l.pos - pos;	// vector towards light
	float l_distance = length(lv);
	//	if (l_distance > l.range) return float3(.0, .0, .0);
	//attenuation
	return  max(dot(lv / l_distance, normalize(normal)), 0.f) * l.diffuse * col / dot(l.att_range.xyz, float3(1.0f, l_distance, l_distance * l_distance));
}
