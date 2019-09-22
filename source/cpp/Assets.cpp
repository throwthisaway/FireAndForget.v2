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
	Concurrency::task<void> Assets::LoadContext::LoadMesh(const wchar_t* fname, size_t id) {
		return DX::ReadDataAsync(fname).then([this, fname, id](std::vector<byte>& data) {
			createModelResults[id] = CreateModel(fname, data);
		});
	}
	Concurrency::task<void> Assets::LoadContext::LoadImage(const wchar_t* fname, size_t id) {
		return DX::ReadDataAsync(fname).then([type = GetImageFileType(fname, wcslen(fname)), this, id](std::vector<byte>& data) {
			images[id] = std::move(DecodeImageFromData(data, type));
		});
	}
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
		ModoMesh mesh = { ws2s(fname), renderer->CreateBuffer(res.vertices.data(), res.vertices.size()),
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
		models.resize(STATIC_MODEL_COUNT);
		textures.resize(STATIC_IMAGE_COUNT);
#ifdef PLATFORM_WIN
		loadContext.images.resize(STATIC_IMAGE_COUNT);
		loadContext.imageLoadTasks.push_back(loadContext.LoadImage(L"random.png", RANDOM));
		loadContext.imageLoadTasks.push_back(loadContext.LoadImage(L"Alexs_Apt_2k.hdr", ENVIRONMENT_MAP));
		loadContextModo.createModelResults.resize(STATIC_MODEL_COUNT);
		loadContext.createModelResults.resize(STATIC_MODEL_COUNT);
		std::initializer_list<Concurrency::task<void>> loadMeshTasks{
			loadContext.LoadMesh(L"light.mesh", LIGHT),
			loadContext.LoadMesh(L"box.mesh", PLACEHOLDER),
			loadContext.LoadMesh(L"checkerboard.mesh", CHECKERBOARD),
			loadContext.LoadMesh(L"BEETHOVE_object.mesh", BEETHOVEN),
			loadContext.LoadMesh(L"sphere.mesh", SPHERE),
			loadContext.LoadMesh(L"textured_unit_cube.mesh", UNITCUBE),

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
			loadContextModo.LoadMesh(L"box_normal_map_test.mesh"),
			//loadContextModo.LoadMesh(L"BEETHOVE_subdivided_twice.mesh"),
			loadContextModo.LoadMesh(L"BEETHOVE_merged_subdivided_twice.mesh"),
		};
		
		Concurrency::when_all(std::begin(loadMeshTasks), std::end(loadMeshTasks)).then([this]() {
			return (Concurrency::when_all(std::begin(loadContext.imageLoadTasks), std::end(loadContext.imageLoadTasks)) && 
				Concurrency::when_all(std::begin(loadContextModo.imageLoadTasks), std::end(loadContextModo.imageLoadTasks))).then([this]() {
				return status = Status::kLoaded;
			}); });
#elif defined(PLATFORM_MAC_OS)
		loadContext.images.resize(STATIC_IMAGE_COUNT);
		loadContext.images[RANDOM] = assets::Assets::LoadImage(L"random.png");
		loadContext.images[ENVIRONMENT_MAP] =  assets::Assets::LoadImage(L"Alexs_Apt_2k.hdr"/*L"Serpentine_Valley_3k.hdr"*/);
		renderer->BeginUploadResources();
		LoadMesh(renderer, L"light.mesh", LIGHT);
		LoadMesh(renderer, L"box.mesh", PLACEHOLDER);
		LoadMesh(renderer, L"checkerboard.mesh", CHECKERBOARD);
		LoadMesh(renderer, L"BEETHOVE_object.mesh", BEETHOVEN);
		LoadMesh(renderer, L"sphere.mesh", SPHERE);
		LoadMesh(renderer, L"textured_unit_cube.mesh", UNITCUBE);
		materials = std::move(loadContext.materials);
		ImagesToTextures(renderer);
		renderer->EndUploadResources();
		loadContext = LoadContext{};
		loadContextModo.meshes.resize(STATIC_MODEL_COUNT);
		LoadModoMesh(renderer, L"textured_unit_cube_modo.mesh", UNITCUBE);
//		LoadModoMesh(renderer, L"light_modo.mesh", LIGHT);
//		LoadModoMesh(renderer, L"box_modo.mesh", PLACEHOLDER);
//		LoadModoMesh(renderer, L"checkerboard_modo.mesh", CHECKERBOARD);
//		LoadModoMesh(renderer, L"BEETHOVE_object_modo.mesh", BEETHOVEN);
//		LoadModoMesh(renderer, L"sphere_modo.mesh", SPHERE);
		LoadModoMesh(renderer, L"test_torus.mesh");
		LoadModoMesh(renderer, L"checkerboard_modo.mesh");

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
	void Assets::ImagesToTextures(Renderer* renderer) {
		for (int id = 0; id < (int)loadContext.images.size(); ++id) {
			auto& img = loadContext.images[id];
			if (textures.size() <= id) textures.resize(id + 1);
			textures[id] = renderer->CreateTexture(img.data.get(), img.width, img.height, img.pf);
		}
		// replace image ids with texture ids
		for (auto& m : models)
			for (auto& l : m.layers)
				for (auto& s : l.submeshes) if (s.texAlbedo != InvalidTexture) s.texAlbedo = textures[s.texAlbedo];
	}
	void Assets::Update(Renderer* renderer) {
#if defined(PLATFORM_WIN)
		if (status == Status::kLoaded) {
			std::copy(std::begin(loadContext.materials), std::end(loadContext.materials), std::back_inserter(materials));
			renderer->BeginUploadResources();
			for (int id = 0; id < (int)loadContext.createModelResults.size(); ++id) {
				auto& res = loadContext.createModelResults[id];
				models[id] = std::move(res.mesh);
				models[id].vb = renderer->CreateBuffer(res.vb.data(), res.vb.size());
				models[id].ib = renderer->CreateBuffer(res.ib.data(), res.ib.size());
			}
			ImagesToTextures(renderer);
			loadContext = LoadContext();

			loadContextModo.meshes.resize(loadContextModo.createModelResults.size());
			for (int id = 0; id < (int)loadContextModo.createModelResults.size(); ++id) {
				auto& res = loadContextModo.createModelResults[id];
				if (res.submeshes.empty()) continue;
				loadContextModo.meshes[id] = { std::move(res.name), renderer->CreateBuffer(res.vertices.data(), res.vertices.size()),
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
	Assets::LoadContext::CreateModelResult Assets::LoadContext::CreateModel(const wchar_t* name, const std::vector<uint8_t>& data) {
		MeshLoader::Mesh mesh;
		mesh.data = std::move(data);
		MeshLoader::LoadMesh(mesh.data.data(), mesh.data.size(), mesh);
		Mesh result;
		std::vector<uint8_t> vb, ib;
		for (const auto& layer : mesh.layers) {
			result.layers.push_back({});
			Layer& l = result.layers.back();
			l.pivot = { layer.pivot.x, layer.pivot.y, layer.pivot.z };
			for (size_t i = 0; i < layer.poly.count; ++i) {
				auto section = layer.poly.sections[i];
				auto surfaceIndex = section.index;
				auto surf = mesh.surfaces[surfaceIndex];
				auto colLayers = surf.surface_infos[MeshLoader::COLOR_MAP].layers;

				auto res = materialMap.insert({ std::wstring{ name } +L'_' + std::wstring{ s2ws(surf.name) }, (MaterialIndex)materials.size() });
				MaterialIndex materialIndex = res.first->second;
				auto vOffset = (offset_t)vb.size(), iOffset = (offset_t)ib.size();
				l.submeshes.push_back({ materialIndex, InvalidTexture, vOffset, iOffset, 0/*stride*/, 0/*count*/, VertexType::PN });	
				if (res.second) {
					materials.push_back({});
					Material material{};
					material.diffuse = { surf.color[0], surf.color[1], surf.color[2] };
					material.metallic_roughness = { surf.surface_infos[MeshLoader::SPECULARITY_MAP].val,
						surf.surface_infos[MeshLoader::GLOSSINESS_MAP].val };
					materials[materialIndex] = material;
					if (colLayers && colLayers->image && colLayers->image->path) {
						l.submeshes.back().vertexType = VertexType::PNT;
						auto inserted = imageMap.insert({ s2ws(colLayers->image->path), (TextureIndex)images.size() });
						l.submeshes.back().texAlbedo = inserted.first->second;
						if (inserted.second) {
							auto path = s2ws(colLayers->image->path);
							auto pos = path.rfind(L'\\');
							if (pos == std::wstring::npos) pos = 0;
							else ++pos;
							path = path.substr(pos);
#if defined(PLATFORM_WIN)
							images.push_back({});
							imageLoadTasks.push_back(LoadImage(path.c_str(), inserted.first->second));
#elif defined(PLATFORM_MAC_OS)
							images.push_back(LoadImage(path.c_str()));
#endif
						}
					}
				}
				if (l.submeshes.back().vertexType == VertexType::PN) {
					MeshGen<VertexPN> pn;
					l.submeshes.back().stride = sizeof(decltype(pn)::VertexType);
					for (MeshLoader::index_t i = section.offset, end = section.offset + section.count; i < end; ++i) {
						auto& p = mesh.polygons[i];
						auto& n = mesh.normalsPV[i];
						pn.unindexedVB.push_back({ mesh.vertices[p.v1], n.n[0] });
						pn.unindexedVB.push_back({ mesh.vertices[p.v2], n.n[1] });
						pn.unindexedVB.push_back({ mesh.vertices[p.v3], n.n[2] });
					}
					pn.Remap();
					l.submeshes.back().count = (index_t)pn.indices.size();
					auto vSize = pn.GetVerticesByteSize(), iSize = pn.GetIndicesByteSize();
					vb.resize(vb.size() + AlignTo<16>(vSize));
					ib.resize(ib.size() + iSize);
					memcpy(vb.data() + vOffset, pn.vertices.data(), vSize);
					memcpy(ib.data() + iOffset, pn.indices.data(), iSize);
				}
				else if (l.submeshes.back().vertexType == VertexType::PNT) {
					MeshGen<VertexPNT> pnt;
					l.submeshes.back().stride = sizeof(decltype(pnt)::VertexType);
					const auto& uv = mesh.uvs.uvmaps[surfaceIndex][colLayers->uvmap];
					for (MeshLoader::index_t i = section.offset, j = 0, end = section.offset + section.count; i < end; ++i) {
						auto& p = mesh.polygons[i];
						auto& n = mesh.normalsPV[i];
						pnt.unindexedVB.push_back({ mesh.vertices[p.v1], n.n[0], {uv.uv[j++], uv.uv[j++]} });
						pnt.unindexedVB.push_back({ mesh.vertices[p.v2], n.n[1], {uv.uv[j++], uv.uv[j++]} });
						pnt.unindexedVB.push_back({ mesh.vertices[p.v3], n.n[2], {uv.uv[j++], uv.uv[j++]} });
					}
					pnt.Remap();
					l.submeshes.back().count = (index_t)pnt.indices.size();
					auto vSize = pnt.GetVerticesByteSize(), iSize = pnt.GetIndicesByteSize();
					vb.resize(vb.size() + AlignTo<16>(vSize));
					ib.resize(ib.size() + iSize);
					memcpy(vb.data() + vOffset, pnt.vertices.data(), vSize);
					memcpy(ib.data() + iOffset, pnt.indices.data(), iSize);
				}
			}
		}

		//result.vb = renderer->CreateBuffer(vb.data(), vb.size());
		//result.ib = renderer->CreateBuffer(ib.data(), ib.size());
		return { result, std::move(vb), std::move(ib) };
	}
}
