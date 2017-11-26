#pragma once
#include "RendererWrapper.h"
#include <array>
#include "MeshLoader.h"
#include <glm\glm.hpp>
#if defined(PLATFORM_WIN)
#include <ppltasks.h>
#endif
struct SubMesh {
	// TOD:: uint16_t is not enough
	MeshLoader::index_t offset, count;
};
struct Mesh {
	size_t vb, colb, ib;
	struct Layer {
		glm::vec3 pivot;
		std::vector<SubMesh> submeshes;
	};
	std::vector<Layer> layers;
};
struct Assets {
	~Assets();
	void Init(RendererWrapper* renderer);
	static constexpr size_t PLACEHOLDER1 = 0;
	static constexpr size_t PLACEHOLDER2 = 1;
	static constexpr size_t CHECKERBOARD = 2;
	static constexpr size_t BEETHOVEN = 3;
	static constexpr size_t STATIC_MODEL_COUNT = 4;
	std::array<Mesh, STATIC_MODEL_COUNT> staticModels;
	bool loadCompleted = false;
#if defined(PLATFORM_WIN)
	Concurrency::task<void> completionTask;
#endif
};
