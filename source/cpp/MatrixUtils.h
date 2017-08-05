#pragma once
#include <glm/gtc/matrix_transform.hpp>

inline glm::mat4 RotationMatrix(const glm::mat4& mat, float radX, float radY, float radZ) {
	return glm::rotate(glm::rotate(glm::rotate(mat, radX, {1.f, 0.f, 0.f}), radY, {0.f, 1.f, 0.f}),	radZ, {0.f, 0.f, 1.f});
}
