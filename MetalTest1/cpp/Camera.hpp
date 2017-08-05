#pragma once
#include <glm/glm.hpp>

struct Camera {
	const float fovy = 45.f, near = .1f, far = 100.f;
	glm::mat4 view, proj, vp, ivp, rotMat;
	glm::vec3 pos, center, rot;
	void Perspective(size_t w, size_t h);
	void Update();
	void Rotate(float dh, float dp, float db = 0.f, const glm::vec3& center = {});
};
