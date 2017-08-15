#pragma once
#include "RendererWrapper.h"
#include <array>
struct Model {
	size_t vertices, color, index, count;
};
struct Assets {
	void Init(RendererWrapper* renderer);
	static constexpr size_t PLACEHOLDER1 = 0;
	static constexpr size_t PLACEHOLDER2 = 1;
	static constexpr size_t STATIC_MODEL_COUNT = 2;
	std::array<Model, STATIC_MODEL_COUNT> staticModels;
};
