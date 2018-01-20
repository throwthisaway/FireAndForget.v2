#pragma once
#include "RendererWrapper.h"
#include <array>
#include <unordered_map>
#include <glm/glm.hpp>
#include "MeshLoader.h"
#include "Img.h"
#include "ShaderStructures.h"
#if defined(PLATFORM_WIN)
#include <ppltasks.h>
#endif

struct Material {
	// pos
	ShaderResourceIndex cColor = InvalidShaderResource;
	// tex
	BufferIndex tStaticColorTexture = InvalidBuffer, staticColorUV = InvalidBuffer;
	ShaderResourceIndex cMaterial = InvalidShaderResource;
};
// geometry by surface
struct SubMesh {
	// TODO:: uint16_t is not enough
	MeshLoader::index_t offset, count;
	Material& material;
};
struct Mesh {
	BufferIndex vb = InvalidBuffer, colb = InvalidBuffer, ib = InvalidBuffer, nb = InvalidBuffer;
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
	std::unordered_map<std::wstring, Img::ImgData> images;
	std::unordered_map<std::wstring, Material> materials;
#if defined(PLATFORM_WIN)
	std::vector<Concurrency::task<void>> loadTasks;
	Concurrency::task<void> loadCompleteTask;
#endif
private:
	void CreateModel(const wchar_t* name, RendererWrapper* renderer, Mesh& model, MeshLoader::Mesh& mesh);
};
