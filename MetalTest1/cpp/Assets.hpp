#pragma once
#include "RendererWrapper.h"

struct Model {
	size_t vertices, color, count;
};
struct Assets {
	void Init(RendererWrapper* renderer);
	// TODO:: cleanup models
	Model placeholder1_, placeholder2_;
};
