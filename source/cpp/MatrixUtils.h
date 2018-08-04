#pragma once
//#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>

inline glm::mat4 RotationMatrix(float angleX, float angleY, float angleZ) {
	return glm::rotate(glm::rotate(glm::rotate(glm::mat4{}, angleX, {1.f, 0.f, 0.f}), angleY, {0.f, 1.f, 0.f}),	angleZ, {0.f, 0.f, 1.f});
}

inline glm::mat4 ScreenSpaceRotator(const glm::mat4& in, const Transform& transform) {
	glm::mat4 mat;
	mat = RotationMatrix(transform.rot.x, transform.rot.y, transform.rot.z);
	mat[3] = glm::vec4(transform.pos + transform.center, 1.f);
	return glm::translate(mat * glm::mat4(glm::mat3(in)), -transform.center);
}

inline glm::vec3 EyePosFromViewNoScale(const glm::mat4& view) {
	glm::mat3 rotMat(view);
	glm::vec3 pos = view[3];
	return -pos * rotMat;
}
