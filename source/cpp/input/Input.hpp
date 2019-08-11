#pragma once
#include "SIMDTypeAliases.h"

class Input {
	float x_, y_;
public:
	float3 dpos, drot;
	void Start(float x, float y);
	float3 Rotate(float x, float y);
	float3 TranslateXZ(float x, float y);
	float3 TranslateY(float y);
};
