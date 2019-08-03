#include "pch.h"
#include "Camera.hpp"
#include "MatrixUtils.h"
//#include <iostream>
//#include <iomanip>

void Camera::Perspective(size_t w, size_t h) {
	proj = glm::perspectiveFovLH_ZO(fovy, (float)w, (float)h, n, f);
	ip = glm::inverse(proj);
}

void Camera::Translate(const vec3_t& d) {
	pos += d;
	at += d;
	//std::cout<<std::setw(10)<<std::setprecision(8)<<this->pos.x<<std::setw(10)<<std::setprecision(8)<<this->pos.y<<std::setw(10)<<std::setprecision(8)<<this->pos.z<<std::endl;
}

void Camera::Rotate(const vec3_t& d) {
	rot += d;
}

void Camera::Update() {
	view = glm::lookAtLH(pos, at, glm::vec3{0.f, 1.f, 0.f});
	// premultiply with rotation to rotate
	auto mat = RotationMatrix(rot.x, rot.y, rot.z);
	auto pos = view[3];
	view = glm::mat4(glm::mat3(mat) * glm::mat3(view));
	view[3] = pos;
	
	vp = proj*view;
	ivp = glm::inverse(vp);
}
