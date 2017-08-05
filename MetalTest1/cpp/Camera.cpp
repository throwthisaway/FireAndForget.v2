#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
void Camera::Perspective(size_t w, size_t h) {
	proj = glm::perspective(fovy, (float)w / h, near, far);
}

void Camera::Update() {
	const float pixelToRadRatio = .01f;
	view = glm::rotate(glm::rotate(glm::rotate(glm::translate({}, pos + center), rot.x * pixelToRadRatio, {1.f, 0.f, 0.f}),
										 rot.y * pixelToRadRatio, {0.f, 1.f, 0.f}),
							 rot.z * pixelToRadRatio, {0.f, 0.f, 1.f});

	view *= rotMat;
	rot = {};
	// copy topleft 3x3 matrix
	rotMat[0] = view[0]; rotMat[1] = view[1]; rotMat[2] = view[2];
	vp = proj*view;
	view = glm::translate(view, -center);
	ivp = glm::inverse(vp);
}

void Camera::Rotate(float dh, float dp, float db, const glm::vec3& center) {
	rot.x += dh; rot.y += dp; rot.z += db;
}
