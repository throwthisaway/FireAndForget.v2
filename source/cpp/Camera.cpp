#include "pch.h"
#include "Camera.hpp"
#include "MatrixUtils.h"
#include <iostream>
namespace {
	const float fovy = 45.f, n = .1f, f = 100.f;
}
void Camera::Perspective(size_t w, size_t h) {
	proj = glm::perspective(fovy, (float)w / h, n, f);
}

void Camera::Update() {
	const float pixelToRadRatio = .01f;
	view = RotationMatrix(glm::translate({}, transform.pos + transform.center), transform.rot.x * pixelToRadRatio, transform.rot.y * pixelToRadRatio, transform.rot.z * pixelToRadRatio);

	view *= rotMat;
	transform.rot = {};
	// copy topleft 3x3 matrix
	rotMat[0] = view[0]; rotMat[1] = view[1]; rotMat[2] = view[2];
	vp = proj*view;
	view = glm::translate(view, -transform.center);
	ivp = glm::inverse(vp);
}

