#pragma once
#include <glm/glm.hpp>

struct Transform;

class ScreenSpaceRotator {
	glm::mat3 rot;
public:
	glm::mat4 Update(const Transform& transform);
};
