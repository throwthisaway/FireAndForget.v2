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
namespace assets {
	struct VertexPUV {
		vec2_t pos, uv;
	};
	struct VertexPN {
		vec3_t pos, n;
	};
	struct VertexPNT {
		vec3_t pos, n;
		vec2_t uv;
	};

	struct Material {
		vec3_t albedo;
		float metallic, roughness;
		TextureIndex texAlbedo;
	};
	struct Submesh {
		MaterialIndex material;
		offset_t vbByteOffset, ibByteOffset, stride;
		index_t count;
		ShaderId id;
	};
	struct Layer {
		vec3_t pivot;
		std::vector<Submesh> submeshes;
	};
	struct Mesh {
		BufferIndex vb = InvalidBuffer, ib = InvalidBuffer;
		std::vector<Layer> layers;
	};
	struct Assets {
		~Assets();
		void Init(RendererWrapper* renderer);
		static constexpr size_t LIGHT = 0;
		static constexpr size_t PLACEHOLDER = 1;
		static constexpr size_t CHECKERBOARD = 2;
		static constexpr size_t BEETHOVEN = 3;
		static constexpr size_t SPHERE = 4;
		static constexpr size_t STATIC_MODEL_COUNT = 5;
		std::vector<Mesh> models;
		std::unordered_map<std::wstring, Img::ImgData> images;
		std::unordered_map<std::wstring, MaterialIndex> materialMap;
		std::vector<Material> materials;
#if defined(PLATFORM_WIN)
		std::vector<Concurrency::task<void>> loadTasks;
		Concurrency::task<void> loadCompleteTask;
#endif
	private:
		using ImageLoadCB = std::function<void(bool, Img::ImgData&)>;
		void LoadFromBundle(const char * path, const ImageLoadCB&);
		Mesh CreateModel(const wchar_t* name, RendererWrapper* renderer, MeshLoader::Mesh& mesh);
#if defined(PLATFORM_WIN)
		Concurrency::task<void> LoadMesh(RendererWrapper* renderer, const wchar_t* fname, size_t id);
#elif defined(PLATFORM_MAC_OS)
		void LoadMesh(RendererWrapper* renderer, const wchar_t* fname, size_t id);
#endif
	};
}