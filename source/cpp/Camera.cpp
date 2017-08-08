#include "Camera.hpp"
#include "MatrixUtils.h"
#include <iostream>
void Camera::Perspective(size_t w, size_t h) {
	proj = glm::perspective(fovy, (float)w / h, near, far);
}

void Camera::Update() {
	const float pixelToRadRatio = .01f;
	view = RotationMatrix(glm::translate({}, pos + center), rot.x * pixelToRadRatio, rot.y * pixelToRadRatio, rot.z * pixelToRadRatio);

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

void Camera::Translate(float dx, float dy, float dz) {
	const float pixelToTrRatio = .01f;
	pos.x += dx * pixelToTrRatio; pos.y += dy * pixelToTrRatio; pos.z += dz * pixelToTrRatio;
}
