#include "pch.h"
#include "Camera.hpp"
#include "MatrixUtils.h"
#include <iostream>

void Camera::Perspective(size_t w, size_t h) {
	proj = glm::perspective(fovy, (float)w / h, n, f);
	ip = glm::inverse(proj);
}

void Camera::Translate(const vec3_t& pos) {
	view = glm::translate(glm::identity<glm::mat4x4>(), pos);
}

void Camera::RotatePreMultiply(const vec3_t& rot) {
	auto mat = RotationMatrix(rot.x, rot.y, rot.z);
	auto pos = view[3];
	view = glm::mat4(glm::mat3(mat) * glm::mat3(view));
	view[3] = pos;
}

vec3_t Camera::GetEyePos() const{
	// no scale
	glm::mat3 rotMat(view);
	glm::vec3 pos = glm::vec3(view[3]);
	return -pos * rotMat;
}

void Camera::Update() {
	vp = proj*view;
	ivp = glm::inverse(vp);
}
