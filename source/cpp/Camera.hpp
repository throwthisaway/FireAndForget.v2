#pragma once
#include <glm/glm.hpp>
#include "Transform.h"

struct Camera {
	glm::mat4 view, proj, vp, ivp;
	Transform transform;
	void Perspective(size_t w, size_t h);
	void Update();
};
