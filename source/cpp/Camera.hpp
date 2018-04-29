#pragma once
#include <glm/glm.hpp>
#include "Transform.h"

struct Camera {
	const float fovy = 45.f, n = .1f, f = 100.f;
	glm::mat4 view, proj, vp, ivp;
	Transform transform;
	void Perspective(size_t w, size_t h);
	void Update();
};
