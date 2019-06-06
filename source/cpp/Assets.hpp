#pragma once
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
#endif
namespace assets {
	struct Assets {
		~Assets();
		void Init();
		void Update(Renderer* renderer);
		static constexpr size_t LIGHT = 0;
		static constexpr size_t PLACEHOLDER = 1;
		static constexpr size_t CHECKERBOARD = 2;
		static constexpr size_t BEETHOVEN = 3;
		static constexpr size_t SPHERE = 4;
		static constexpr size_t UNITCUBE = 5;
		static constexpr size_t STATIC_MODEL_COUNT = 6;
		std::vector<Mesh> models;
		static constexpr size_t RANDOM = 0;
		static constexpr size_t ENVIRONMENT_MAP = 1;
		static constexpr size_t STATIC_IMAGE_COUNT = 2;
		std::vector<TextureIndex> textures;
		std::vector<Material> materials;
		enum class Status { kInitialized, kLoaded, kReady } status;
	private:
		struct LoadContext {
			struct CreateModelResult {
				Mesh mesh;
				std::vector<uint8_t> vb, ib;
			};
			CreateModelResult CreateModel(const wchar_t* name, const std::vector<uint8_t>& data);
			Concurrency::task<void> LoadMesh(const wchar_t* fname, size_t id);
			Concurrency::task<void> LoadImage(const wchar_t* fname, size_t id);
#if defined(PLATFORM_WIN)
			concurrency::concurrent_unordered_map<std::wstring, TextureIndex> imageMap;
			concurrency::concurrent_vector<Img::ImgData> images;
			concurrency::concurrent_unordered_map<std::wstring, MaterialIndex> materialMap;
			concurrency::concurrent_vector<Material> materials;
			concurrency::concurrent_vector<CreateModelResult> createModelResults;
			concurrency::concurrent_vector<Concurrency::task<void>> imageLoadTasks;
#elif defined(PLATFORM_MAC_OS)
			std::unordered_map<std::wstring, TextureIndex> imageMap;
			std::vector<Img::ImgData> images;
			std::unordered_map<std::wstring, MaterialIndex> materialMap;
			std::vector<Material> materials;
#endif
		}loadContext;
#if defined(PLATFORM_WIN)
#elif defined(PLATFORM_MAC_OS)
		void LoadMesh(Renderer* renderer, const wchar_t* fname, size_t id);
		static Img::ImgData LoadImage(const wchar_t* fname);
#endif
		void ImagesToTextures(Renderer*);
	};
}
