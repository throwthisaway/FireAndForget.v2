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
	view = ssRot.Update(transform);
	transform.rot = {};
	vp = proj*view;
	ivp = glm::inverse(vp);
}

