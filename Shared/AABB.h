#pragma once
#include "SIMDTypeAliases.h"
#include <limits>

struct AABB{
	float3 min = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() },
		max = { std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min() };
	void Add(const AABB& aabb) {
		if (aabb.min.x < min.x) min.x = aabb.min.x;
		if (aabb.min.y < min.y) min.y = aabb.min.y;
		if (aabb.min.z < min.z) min.z = aabb.min.z;
		if (aabb.max.x > max.x) max.x = aabb.max.x;
		if (aabb.max.y > max.y) max.y = aabb.max.y;
		if (aabb.max.z > max.z) max.z = aabb.max.z;
	}
};