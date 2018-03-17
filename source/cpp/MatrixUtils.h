#pragma once
#include <glm/gtc/matrix_transform.hpp>

inline glm::mat4 RotationMatrix(const glm::mat4& mat, float radX, float radY, float radZ) {
	return glm::rotate(glm::rotate(glm::rotate(mat, radX, {1.f, 0.f, 0.f}), radY, {0.f, 1.f, 0.f}),	radZ, {0.f, 0.f, 1.f});
}
// TODO:: sort out what is center used forthen rename it to a more generic/appropriate name
inline glm::mat4 ScreenSpaceRotator(const glm::mat4& in, const Transform& transform) {
	glm::mat4 mat = RotationMatrix(glm::translate({}, transform.pos + transform.center), transform.rot.x, transform.rot.y, transform.rot.z);
	return glm::translate(mat * glm::mat4(glm::mat3(in)), -transform.center);
}
