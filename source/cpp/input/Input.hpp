#pragma once
#include <glm/glm.hpp>
#include "../Transform.h"

class Input {
	float x_, y_;
public:
	void Start(float x, float y);
	void Rotate(float x, float y, Transform& transform);
	void TranslateXZ(float x, float y, Transform& transform);
	void TranslateY(float y, Transform& transform);
};
