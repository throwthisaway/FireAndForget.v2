#include "pch.h"
#include "Camera.hpp"
#include "MatrixUtils.h"
#include <iostream>

void Camera::Perspective(size_t w, size_t h) {
	proj = glm::perspective(fovy, (float)w / h, n, f);
}

void Camera::Update() {
	vp = proj*view;
	ivp = glm::inverse(vp);
}
