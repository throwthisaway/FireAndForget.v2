#pragma once
#include <glm/glm.hpp>
#include "Transform.h"

struct Camera {
	const float fovy = 45.f, n = .1f, f = 100.f;
	const vec3_t defaultPos{0.f, 0.f, -15.5f};
	glm::mat4 view{1.f}, proj, vp, ivp, ip;
	vec3_t pos = defaultPos, at, rot;
	void Translate(const vec3_t& d);
	void Rotate(const vec3_t& d);
	void Perspective(size_t w, size_t h);
	void Update();
};
