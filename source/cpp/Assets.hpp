#pragma once
#include "RendererWrapper.h"
#include <array>
#include <glm\glm.hpp>
#include "MeshLoader.h"
#include "Img.h"
#if defined(PLATFORM_WIN)
#include <ppltasks.h>
#endif
struct SubMesh {
	// TODO:: uint16_t is not enough
	MeshLoader::index_t offset, count;
	uint16_t material;
};
struct Mesh {
	size_t vb, colb, ib;
	struct Layer {
		glm::vec3 pivot;
		std::vector<SubMesh> submeshes;
	};
	std::vector<Layer> layers;
};
struct Material {
	glm::vec3 color;
	Img::ImgData* color_image = nullptr;
	float specular, power, alpha;
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
	std::vector<Img::ImgData> images;
	std::vector<Material> materials;
	bool loadCompleted = false;
#if defined(PLATFORM_WIN)
	std::vector<Concurrency::task<void>> loadTasks;
#endif
private:
	void CreateModel(RendererWrapper* renderer, Mesh& model, MeshLoader::Mesh& mesh);
};
