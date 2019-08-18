#pragma once
#include <glm/glm.hpp>
#include "Transform.h"

struct Camera {
	const float fovy = 45.f, n = .1f, f = 100.f;
	const float3 defaultPos{0.f, 0.f, -10.5f};
	glm::mat4 view{1.f}, proj, vp, ivp, ip;
	float3 pos = defaultPos, at, rot, eyePos;
	void Translate(const float3& d);
	void Rotate(const float3& d);
	void Perspective(size_t w, size_t h);
	void Update();
};
