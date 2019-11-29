#include "pch.h"
#include "compatibility.h"
#include "Assets.hpp"
#include <assert.h>
#include "SIMDTypeAliases.h"
#include "VertexTypes.h"
#ifdef PLATFORM_MAC_OS
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFStream.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFByteOrder.h>
#elif defined(PLATFORM_WIN)
#include "Common\DirectXHelper.h"
#include "Content\DescriptorHeapAllocator.h"
#endif
#include "MeshLoader.h"
#include "Tga.h"
#include "StringUtil.h"
#include "BufferUtils.h"
#define STB_IMAGE_IMPLEMENTATION
#ifdef PLATFORM_MAC_OS
#include "../../../3rdparty/meshoptimizer/src/meshoptimizer.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#include "../../../3rdparty/stb/stb_image.h"
#pragma clang diagnostic pop
#else
#include "meshoptimizer.h"
#include "stb_image.h"
#endif
//#define INDEXED_DRAWING
namespace {
#ifdef PLATFORM_MAC_OS
	std::vector<uint8_t> LoadFromBundle(const wchar_t* fname) {
		std::vector<uint8_t> data;
		CFBundleRef mainBundle = CFBundleGetMainBundle();
		assert(mainBundle);
		CFStringEncoding encoding = (CFByteOrderLittleEndian == CFByteOrderGetCurrent()) ? kCFStringEncodingUTF32LE : kCFStringEncodingUTF32BE;

		auto widecharvarLen = wcslen(fname);

		CFStringRef string = CFStringCreateWithBytes(NULL,
													 reinterpret_cast<const uint8_t*>(fname),
													 (widecharvarLen * sizeof(wchar_t)),
													 encoding,
													 false);
		CFURLRef url = CFBundleCopyResourceURL(mainBundle, string, NULL, NULL);
		CFRelease(string);
		//	CFUrlRef --> const char*
		//	CFStringRef pathURL = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
		//
		//	CFStringEncoding encodingMethod = CFStringGetSystemEncoding();
		//	const char *path = CFStringGetCStringPtr(pathURL, encodingMethod);
		//	CFRelease(pathURL);
		//	CFRelease(url);

		CFErrorRef error = 0;
		CFNumberRef fileSizeRef = 0;
		signed long long int fileSize = 0;
		if (CFURLCopyResourcePropertyForKey(url, kCFURLFileSizeKey, &fileSizeRef, &error) && fileSizeRef) {
			CFNumberGetValue(fileSizeRef, kCFNumberLongLongType, &fileSize);
			CFRelease(fileSizeRef);
		}
		if (!fileSize) {
			CFRelease(url);
			CFRelease(error);
			return data;
		}
		CFReadStreamRef rs = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
		Boolean res = CFReadStreamOpen(rs);
		if (res) {
			data.resize(fileSize);
			CFIndex read = CFReadStreamRead(rs, data.data(), fileSize);
			if (read == fileSize) {
				// all good
			} else {
				// error read != filesize
			}
			CFReadStreamClose(rs);
		} else {
			// cannot open stream
		}
		CFRelease(rs);
		CFRelease(url);
		if (error) CFRelease(error);
		//TODO:: ??? CFRelease(mainBundle);
		return data;
	}
#endif
//	assets::Mesh CreateUnitCube(RendererWrapper* renderer) {
//		const float x = 1.f, y = 1.f, z = 1.f;
//		const float v[] = {x, y, -z,
//							x,-y,-z,
//							-x,y,-z,
//							-x,-y,-z,
//							-x,-y,z,
//							x,-y,z,
//							x,y,z,
//							-x, y, z};
//		const uint16_t i[] = { 3, 2, 0, 0, 1, 3,
//								7, 0, 2, 6, 0, 7,
//								1, 0, 6, 1, 6, 5,
//								3, 1, 5, 5, 4, 3,
//								2, 3, 4, 4, 7, 2,
//								3, 1, 4, 1, 5, 4};
//		assets::Mesh mesh;
//		mesh.vb = renderer->CreateBuffer(v, sizeof(v));
//		mesh.ib = renderer->CreateBuffer(i, sizeof(i));
//		mesh.layers.push_back({{/*pivot*/}, {{ InvalidMaterial, 0, 0, sizeof(assets::VertexPT), sizeof(i) / sizeof(i[0]), EnvCubeMapShader}}});
//		return mesh;
//	}
}
namespace {
	enum class ImageFileType { kUnknown, kTGA, kPNG, kHDR, kJPG };
	bool EndsWith(const wchar_t* str1, size_t length1, const wchar_t* str2, size_t length2) {
		assert(length1 > length2);
		return !istrcmp(str1, length1 - length2, str2);
	}
	ImageFileType GetImageFileType(const wchar_t* fname, size_t length) {
		if (EndsWith(fname, length, L"tga", 3)) return ImageFileType::kTGA;
		else if (EndsWith(fname, length, L"png", 3)) return ImageFileType::kPNG;
		else if (EndsWith(fname, length, L"hdr", 3)) return ImageFileType::kHDR;
		else if (EndsWith(fname, length, L"jpg", 3)) return ImageFileType::kJPG;
		assert(false && "invalid image file format");
		return ImageFileType::kUnknown;
	}
	Img::ImgData DecodeTGA(const std::vector<uint8_t>& data) {
		Img::ImgData result;
		auto err = Img::DecodeTGA(data.data(), data.size(), result, true);
		assert(err == Img::TgaDecodeResult::Ok);
		return result;
	}
	//struct StbDeleter {
	//	void operator()(uint8_t* p) { stbi_image_free((stbi_uc*)p);	}
	//};
	Img::ImgData StbDecodeImageFromData(const std::vector<uint8_t>& data, Img::PixelFormat pf) {
		int w, h, channels;
		uint8_t bytesPerPixel = BytesPerPixel(pf);
		switch (Img::GetComponentType(pf)) {
		case Img::ComponentType::Float: {
			float* image = stbi_loadf_from_memory(data.data(), (int)data.size(), &w, &h, &channels, 4);
			return { std::unique_ptr<uint8_t/*, StbDeleter default deleter should be fine*/>((uint8_t*)image), (uint16_t)w, (uint16_t)h, bytesPerPixel, pf };
		}
		case Img::ComponentType::Byte: {
			stbi_uc* image = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &channels, 4);
			return { std::unique_ptr<uint8_t/*, StbDeleter default deleter should be fine*/>((uint8_t*)image), (uint16_t)w, (uint16_t)h, bytesPerPixel, pf };
		}
		default:
			assert(false && "unhandled component type");
		}
		return {};
	}
	Img::ImgData DecodeImageFromData(const std::vector<uint8_t>& data, ImageFileType type) {
		switch (type) {
			case ImageFileType::kTGA: /*return DecodeTGA(data);*/
			case ImageFileType::kJPG:
			case ImageFileType::kPNG: return StbDecodeImageFromData(data, Img::PixelFormat::RGBA8);
			case ImageFileType::kHDR: return StbDecodeImageFromData(data, Img::PixelFormat::RGBAF32);
			case ImageFileType::kUnknown: assert(false);
		}
		assert(false);
		return {};
	}
}
namespace assets {
	Assets::~Assets() = default;
#ifdef PLATFORM_WIN
	Concurrency::task<void> Assets::ModoLoadContext::LoadImage(const wchar_t* fname, size_t id) {
		return DX::ReadDataAsync(fname).then([type = GetImageFileType(fname, wcslen(fname)), this, id](std::vector<byte>& data) {
			images[id] = std::move(DecodeImageFromData(data, type));
		});
	}
	Concurrency::task<void> Assets::ModoLoadContext::LoadMesh(const wchar_t* fname, size_t id) {
		return DX::ReadDataAsync(fname).then([this, fname, id](std::vector<byte>& data) {
			auto res = ModoMeshLoader::Load(data);
			res.name = ws2s(fname);
			for (auto& s : res.submeshes)
				for (int i = 0; i < _countof(s.textures); ++i) {
					auto& tex = s.textures[i];
					if (!(s.textureMask & (1 << i)))  continue;
					auto& path = res.images[tex.id];
					auto wpath = s2ws(path);
					auto result = imageMap.insert(make_pair(wpath, (TextureIndex)images.size()));
					if (result.second) {
						tex.id = (uint32_t)images.size();
						images.push_back({});
						imageLoadTasks.push_back(LoadImage(wpath.c_str(), result.first->second));
					} else tex.id = (uint32_t)result.first->second;
				}
			if (id == INVALID) createModelResults.push_back(res);
			else createModelResults[id] = res;
		});
	}
#elif defined(PLATFORM_MAC_OS)
	void Assets::LoadMesh(Renderer* renderer, const wchar_t* fname, size_t id) {
		auto data = ::LoadFromBundle(fname);
		auto res = loadContext.CreateModel(fname, data);
		models[id] = std::move(res.mesh);
		models[id].vb = renderer->CreateBuffer(res.vb.data(), res.vb.size());
		models[id].ib = renderer->CreateBuffer(res.ib.data(), res.ib.size());
	}
	void Assets::LoadModoMesh(Renderer* renderer, const wchar_t* fname, size_t id) {
		auto data = ::LoadFromBundle(fname);
		auto res = ModoMeshLoader::Load(data);
		for (auto& s : res.submeshes)
			for (int i = 0; i < sizeof(s.textures)/sizeof(s.textures[0]); ++i) {
				if (!(s.textureMask & (1 << i)))  continue;
				auto& tex = s.textures[i];
				auto& str = res.images[tex.id];
				auto result = loadContextModo.imageMap.insert(make_pair(str, loadContextModo.images.size()));
				if (result.second) {
					tex.id = (uint32_t)loadContextModo.images.size();
					loadContextModo.images.push_back(str);
				} else {
					tex.id = (uint32_t)result.first->second;
				}
			}
		ModoMesh mesh = { ws2s(fname), res.header.r, res.header.aabb, renderer->CreateBuffer(res.vertices.data(), res.vertices.size()),
			renderer->CreateBuffer(res.indices.data(), res.indices.size()),
			std::move(res.submeshes) };
		if (id == INVALID) loadContextModo.meshes.push_back(mesh);
		else loadContextModo.meshes[id] = mesh;
	}
	Img::ImgData Assets::LoadImage(const wchar_t* fname) {
		std::vector<uint8_t> data = ::LoadFromBundle(fname);
		return DecodeImageFromData(data, GetImageFileType(fname, wcslen(fname)));
	}
#endif

	void Assets::Init(Renderer* renderer) {
		status = Status::kInitialized;
		textures.resize(STATIC_IMAGE_COUNT);
#ifdef PLATFORM_WIN
		loadContextModo.images.resize(STATIC_IMAGE_COUNT);
		loadContextModo.imageLoadTasks.push_back(loadContextModo.LoadImage(L"random.png", RANDOM));
		loadContextModo.imageLoadTasks.push_back(loadContextModo.LoadImage(L"Alexs_Apt_2k.hdr", ENVIRONMENT_MAP));
		loadContextModo.imageLoadTasks.push_back(loadContextModo.LoadImage(L"beam.png", BEAM));
		loadContextModo.createModelResults.resize(STATIC_MODEL_COUNT);
		std::initializer_list<Concurrency::task<void>> loadMeshTasks{
			loadContextModo.LoadMesh(L"textured_unit_cube_modo.mesh", UNITCUBE),
			//loadContextModo.LoadMesh(L"light_modo.mesh", LIGHT),
			//loadContextModo.LoadMesh(L"box_modo.mesh", PLACEHOLDER),
			//loadContextModo.LoadMesh(L"checkerboard_modo.mesh", CHECKERBOARD),
			//loadContextModo.LoadMesh(L"BEETHOVE_object_modo.mesh", BEETHOVEN),
			//loadContextModo.LoadMesh(L"sphere_modo.mesh", SPHERE),
			//loadContextModo.LoadMesh(L"test_torus.mesh"),
			//loadContextModo.LoadMesh(L"checkerboard_modo.mesh"),
			//loadContextModo.LoadMesh(L"sphere_modo.mesh"),
			//loadContextModo.LoadMesh(L"modo_ball_test.mesh"),
			//loadContextModo.LoadMesh(L"box_normal_map_test.mesh"),
			//loadContextModo.LoadMesh(L"BEETHOVE_subdivided_twice.mesh"),
			//loadContextModo.LoadMesh(L"BEETHOVE_merged_subdivided_twice.mesh"),
			
			loadContextModo.LoadMesh(L"shadow_test.mesh"),
			loadContextModo.LoadMesh(L"parallax_test.mesh"),
		};
		
		Concurrency::when_all(std::begin(loadMeshTasks), std::end(loadMeshTasks)).then([this]() {
			return Concurrency::when_all(std::begin(loadContextModo.imageLoadTasks), std::end(loadContextModo.imageLoadTasks)).then([this]() {
				return status = Status::kLoaded;
			}); });
#elif defined(PLATFORM_MAC_OS)
		loadContextModo.meshes.resize(STATIC_MODEL_COUNT);
		LoadModoMesh(renderer, L"textured_unit_cube_modo.mesh", UNITCUBE);
//		LoadModoMesh(renderer, L"light_modo.mesh", LIGHT);
//		LoadModoMesh(renderer, L"box_modo.mesh", PLACEHOLDER);
//		LoadModoMesh(renderer, L"checkerboard_modo.mesh", CHECKERBOARD);
//		LoadModoMesh(renderer, L"BEETHOVE_object_modo.mesh", BEETHOVEN);
//		LoadModoMesh(renderer, L"sphere_modo.mesh", SPHERE);
		LoadModoMesh(renderer, L"test_torus.mesh");
		LoadModoMesh(renderer, L"checkerboard_modo.mesh");
		LoadModoMesh(renderer, L"sphere_modo.mesh");
		LoadModoMesh(renderer, L"box_normal_map_test.mesh");
		LoadModoMesh(renderer, L"BEETHOVE_merged_subdivided_twice.mesh");

		// generate textures
		auto offset = textures.size();
		for (auto& fname : loadContextModo.images) {
			auto img = LoadImage(s2ws(fname).c_str());
			textures.push_back(renderer->CreateTexture(img.data.get(), img.width, img.height, img.pf));
		}

		// replace image ids with texture ids
		for (auto& mesh : loadContextModo.meshes)
			for (auto& s : mesh.submeshes)
				for (int i = 0; i < sizeof(s.textures)/sizeof(s.textures[0]); ++i) {
					auto& texture = s.textures[i];
					if (s.textureMask & (1 << i)) {
						// texture.image was replaced with an index into the context's images array
						texture.id = (uint32_t)textures[offset + texture.id];
					} else texture.id = InvalidTexture;
				}
		meshes = std::move(loadContextModo.meshes);
		loadContextModo = ModoLoadContext{};
		status = Status::kReady;
#endif
	}
	void Assets::Update(Renderer* renderer) {
#if defined(PLATFORM_WIN)
		if (status == Status::kLoaded) {
			renderer->BeginUploadResources();
			ImagesToTextures(renderer);
			loadContextModo.meshes.resize(loadContextModo.createModelResults.size());
			for (int id = 0; id < (int)loadContextModo.createModelResults.size(); ++id) {
				auto& res = loadContextModo.createModelResults[id];
				if (res.submeshes.empty()) continue;
				loadContextModo.meshes[id] = { std::move(res.name), res.header.r, res.header.aabb, renderer->CreateBuffer(res.vertices.data(), res.vertices.size()),
					renderer->CreateBuffer(res.indices.data(), res.indices.size()),
					std::move(res.submeshes) };
			}
			auto offset = textures.size();
			for (auto& img : loadContextModo.images) {
				textures.push_back(renderer->CreateTexture(img.data.get(), img.width, img.height, img.pf));
			}

			// replace image ids with texture ids
			for (auto& mesh : loadContextModo.meshes)
				for (auto& s : mesh.submeshes)
					for (int i = 0; i < _countof(s.textures); ++i) {
						auto& texture = s.textures[i];
						if (s.textureMask & (1 << i)) {
							// texture.image was replaced with an index into the context's images array
							texture.id = (uint32_t)textures[offset + texture.id];
						} else texture.id = InvalidTexture;
					}
			for (auto& mesh : loadContextModo.meshes)
				meshes.emplace_back(std::move(mesh));
			loadContextModo = ModoLoadContext();
			renderer->EndUploadResources();
			status = Status::kReady;
		}
#endif
	}
	namespace {
		template <typename T>
		struct MeshGen {
			using VertexType = T;
			std::vector<T> unindexedVB;

			std::vector<index_t> indices;
			std::vector<T> vertices;
			void Remap() {
				if (unindexedVB.empty()) return;
				std::vector<uint32_t> remap(unindexedVB.size());
				size_t vertexCount = meshopt_generateVertexRemap(remap.data(), nullptr, unindexedVB.size(), unindexedVB.data(), unindexedVB.size(), sizeof(T));
				indices.resize(unindexedVB.size());
				meshopt_remapIndexBuffer<index_t>(indices.data(), nullptr, unindexedVB.size(), remap.data());
				vertices.resize(vertexCount);
				meshopt_remapVertexBuffer(vertices.data(), unindexedVB.data(), unindexedVB.size(), sizeof(T), remap.data());
				unindexedVB.clear();
			}
			uint32_t GetVerticesByteSize() const { return uint32_t(sizeof(T) * vertices.size()); }
			uint32_t GetIndicesByteSize() const { return uint32_t(sizeof(index_t) * indices.size()); }
			void MergeInto(uint8_t* vb, uint8_t* ib) {
				memcpy(vb, vertices.data(), GetVerticesByteSize());
				memcpy(ib, indices.data(), GetIndicesByteSize());
			}
		};
	}
	void Assets::ImagesToTextures(Renderer* renderer) {
		for (int id = 0; id < (int)loadContextModo.images.size(); ++id) {
			auto& img = loadContextModo.images[id];
			if (textures.size() <= id) textures.resize(id + 1);
			textures[id] = renderer->CreateTexture(img.data.get(), img.width, img.height, img.pf);
		}
	}
}
