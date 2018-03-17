#pragma once
#include <glm/glm.hpp>

class Input {
	float x_, y_;
public:
	void Start(float x, float y);
	glm::vec3 Rotate(float x, float y);
	glm::vec3 TranslateXZ(float x, float y);
	glm::vec3 TranslateY(float y);
};
