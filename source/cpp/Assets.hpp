#pragma once
#include "RendererWrapper.h"
#include <array>
#include "MeshLoader.h"

struct Model {
	size_t vertices, color, index, offset, count;
};
struct Assets {
	void Init(RendererWrapper* renderer);
	static constexpr size_t PLACEHOLDER1 = 0;
	static constexpr size_t PLACEHOLDER2 = 1;
	static constexpr size_t CHECKERBOARD = 2;
	static constexpr size_t STATIC_MODEL_COUNT = 3;
	std::array<Model, STATIC_MODEL_COUNT> staticModels;
	MeshLoader::Mesh mesh;
	bool loadCompleted = false;
	~Assets();
};
