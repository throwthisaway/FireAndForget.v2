#pragma once
#include <glm/glm.hpp>
#include "Transform.h"

struct Camera {
	const float fovy = 45.f, n = .1f, f = 100.f;
	glm::mat4 view{1.f}, proj, vp, ivp, ip;
	void Translate(const vec3_t& pos);
	void RotatePreMultiply(const vec3_t& rot);
	vec3_t GetEyePos() const;
	void Perspective(size_t w, size_t h);
	void Update();
};
