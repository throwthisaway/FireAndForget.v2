#include "pch.h"
#include "Camera.hpp"
#include "MatrixUtils.h"
//#include <iostream>
//#include <iomanip>

void Camera::Perspective(size_t w, size_t h) {
	proj = glm::perspectiveFovLH_ZO(fovy, (float)w, (float)h, n, f);
	ip = glm::inverse(proj);
}

void Camera::Translate(const float3& d) {
	pos += d;
	at += d;
	//std::cout<<std::setw(10)<<std::setprecision(8)<<this->pos.x<<std::setw(10)<<std::setprecision(8)<<this->pos.y<<std::setw(10)<<std::setprecision(8)<<this->pos.z<<std::endl;
}

void Camera::Rotate(const float3& d) {
	rot += d;
}

void Camera::Update() {
	view = glm::lookAtLH(pos, at, float3{0.f, 1.f, 0.f});
	// premultiply with rotation to rotate
	auto mat = RotationMatrix(rot.x, rot.y, rot.z);
	auto pos = view[3];
	view = float4x4(float3x3(mat) * float3x3(view));
	view[3] = pos;
	eyePos = glm::inverse(view) * float4(this->pos, 1.f);
	vp = proj*view;
	ivp = glm::inverse(vp);
}
