#pragma once
#include <glm/glm.hpp>
#include "ScreenSpaceRotator.h"
#include "Transform.h"

struct Camera {
	glm::mat4 view, proj, vp, ivp;
	ScreenSpaceRotator ssRot;
	Transform transform;
	void Perspective(size_t w, size_t h);
	void Update();
};
