#pragma once
#include "Platform.h"
#include <array>
#include <unordered_map>
#include <glm/glm.hpp>
#include "MeshLoader.h"
#include "Img.h"
#include "ShaderStructures.h"
#if defined(PLATFORM_WIN)
#include "../Content/Renderer.h"
#include <ppltasks.h>
#include <concurrent_vector.h>
#include <concurrent_unordered_map.h>
#elif defined(PLATFORM_MAC_OS)
#include "../Renderer.h"
#endif
#include "ModoMesh.h"
#include "../../ModoMeshLoader/ModoMeshLoader.h"

namespace assets {
	struct Assets {
		~Assets();
		void Init(Renderer* renderer);
		void Update(Renderer* renderer);
		static constexpr size_t INVALID = -1;
		static constexpr size_t LIGHT = 0;
		static constexpr size_t PLACEHOLDER = 1;
		static constexpr size_t CHECKERBOARD = 2;
		static constexpr size_t BEETHOVEN = 3;
		static constexpr size_t SPHERE = 4;
		static constexpr size_t UNITCUBE = 5;
		static constexpr size_t STATIC_MODEL_COUNT = 6;
		std::vector<ModoMesh> meshes;
		static constexpr size_t RANDOM = 0;
		static constexpr size_t ENVIRONMENT_MAP = 1;
		static constexpr size_t BEAM = 2;
		static constexpr size_t STATIC_IMAGE_COUNT = 3;
		std::vector<TextureIndex> textures;
		std::vector<Material> materials;
		enum class Status { kInitialized, kLoaded, kReady } status;
	private:
		struct ModoLoadContext {
#if defined(PLATFORM_WIN)
			concurrency::concurrent_unordered_map<std::wstring, TextureIndex> imageMap;
			concurrency::concurrent_vector<Img::ImgData> images;
			concurrency::concurrent_vector<ModoMesh> meshes;
			concurrency::concurrent_vector<ModoMeshLoader::Result> createModelResults;
			concurrency::concurrent_vector<Concurrency::task<void>> imageLoadTasks;
			Concurrency::task<void> LoadMesh(const wchar_t* fname, size_t id = INVALID);
			Concurrency::task<void> LoadImage(const wchar_t* fname, size_t id);
#elif defined(PLATFORM_MAC_OS)
			std::vector<std::string> images;
			std::unordered_map<std::string, size_t> imageMap;
			std::vector<ModoMesh> meshes;
#endif
		}loadContextModo;
#if defined(PLATFORM_WIN)
#elif defined(PLATFORM_MAC_OS)
		void LoadModoMesh(Renderer* renderer, const wchar_t* fname, size_t id = INVALID);
		static Img::ImgData LoadImage(const wchar_t* fname);
#endif
		void ImagesToTextures(Renderer* renderer);
	};
}
