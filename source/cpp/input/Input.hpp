#pragma once
#include "types.h"

class Input {
	float x_, y_;
public:
	vec3_t dpos, drot;
	void Start(float x, float y);
	vec3_t Rotate(float x, float y);
	vec3_t TranslateXZ(float x, float y);
	vec3_t TranslateY(float y);
};
