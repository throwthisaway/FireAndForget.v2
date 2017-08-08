#pragma once
#include <glm/glm.hpp>
#include "../Camera.hpp"

class Input {
	float x_, y_;
public:
	Camera* camera_ = nullptr;
	void Start(float x, float y);
	void Rotate(float x, float y);
	void TranslateXZ(float x, float y);
	void TranslateY(float y);
};
