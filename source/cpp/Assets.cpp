#include "pch.h"
#include "compatibility.h"
#include "Assets.hpp"
#include <assert.h>
#include "VertexTypeAliases.h"
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
	enum class ImageFileType { kUnknown, kTGA, kPNG, kHDR };
	bool EndsWith(const wchar_t* str1, size_t length1, const wchar_t* str2, size_t length2) {
		assert(length1 > length2);
		return !istrcmp(str1, length1 - length2, str2);
	}
	ImageFileType GetImageFileType(const wchar_t* fname, size_t length) {
		if (EndsWith(fname, length, L"tga", 3)) return ImageFileType::kTGA;
		if (EndsWith(fname, length, L"png", 3)) return ImageFileType::kPNG;
		if (EndsWith(fname, length, L"hdr", 3)) return ImageFileType::kHDR;
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
			case ImageFileType::kTGA: return DecodeTGA(data);
			case ImageFileType::kPNG: return StbDecodeImageFromData(data, Img::PixelFormat::RGBA8);
			case ImageFileType::kHDR: return StbDecodeImageFromData(data, Img::PixelFormat::RGBAF32);
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
#elif defined(PLATFORM_MAC_OS)
	void Assets::LoadMesh(Renderer* renderer, const wchar_t* fname, size_t id) {
		auto data = ::LoadFromBundle(fname);
		auto res = loadContext.CreateModel(fname, data);
		models[id] = std::move(res.mesh);
		models[id].vb = renderer->CreateBuffer(res.vb.data(), res.vb.size());
		models[id].ib = renderer->CreateBuffer(res.ib.data(), res.ib.size());
	}
	Img::ImgData Assets::LoadImage(const wchar_t* fname) {
		std::vector<uint8_t> data = ::LoadFromBundle(fname);
		return DecodeImageFromData(data, GetImageFileType(fname, wcslen(fname)));
	}
#endif

	void Assets::Init() {
		status = Status::kInitialized;
		models.resize(STATIC_MODEL_COUNT);
		textures.resize(STATIC_IMAGE_COUNT);
#ifdef PLATFORM_WIN
		loadContext.images.resize(STATIC_IMAGE_COUNT);
		loadContext.imageLoadTasks.push_back(loadContext.LoadImage(L"random.png", RANDOM));
		loadContext.imageLoadTasks.push_back(loadContext.LoadImage(L"Alexs_Apt_2k.hdr", ENVIRONMENT_MAP));
		loadContext.createModelResults.resize(STATIC_MODEL_COUNT);
		std::initializer_list<Concurrency::task<void>> loadMeshTasks{
			loadContext.LoadMesh(L"light.mesh", LIGHT),
			loadContext.LoadMesh(L"box.mesh", PLACEHOLDER),
			loadContext.LoadMesh(L"checkerboard.mesh", CHECKERBOARD),
			loadContext.LoadMesh(L"BEETHOVE_object.mesh", BEETHOVEN),
			loadContext.LoadMesh(L"sphere.mesh", SPHERE),
			loadContext.LoadMesh(L"textured_unit_cube.mesh", UNITCUBE),
		};
		
		Concurrency::when_all(std::begin(loadMeshTasks), std::end(loadMeshTasks)).then([this]() {
			return Concurrency::when_all(std::begin(loadContext.imageLoadTasks), std::end(loadContext.imageLoadTasks)).then([this]() {
				return status = Status::kLoaded;
			}); });
#elif defined(PLATFORM_MAC_OS)
		renderer->BeginUploadResources();
		LoadMesh(renderer, L"light.mesh", LIGHT);
		LoadMesh(renderer, L"box.mesh", PLACEHOLDER);
		LoadMesh(renderer, L"checkerboard.mesh", CHECKERBOARD);
		LoadMesh(renderer, L"BEETHOVE_object.mesh", BEETHOVEN);
		LoadMesh(renderer, L"sphere.mesh", SPHERE);
		LoadMesh(renderer, L"textured_unit_cube.mesh", UNITCUBE);
		materials = std::move(loadContext.materials);
		Img::ImgData img = assets::Assets::LoadImage(L"random.png", Img::PixelFormat::RGBA8);
		textures[RANDOM] = renderer_->CreateTexture(img.data.get(), img.width, img.height, img.pf);
		Img::ImgData img = assets::Assets::LoadImage(L"Alexs_Apt_2k.hdr"/*L"Serpentine_Valley_3k.hdr"*/, Img::PixelFormat::RGBAF32);
		textures[ENVIRONMENT_MAP] = renderer->CreateTexture(img.data.get(), img.width, img.height, img.pf);
		ImagesToTextures(renderer);
		renderer->EndUploadResources();
		loadContext = LoadContext{};
		status = Status::kAvailable;
#endif
	}
	void Assets::ImagesToTextures(Renderer* renderer) {
		for (int id = 0; id < (int)loadContext.images.size(); ++id) {
			auto& img = loadContext.images[id];
			if (textures.size() <= id) textures.resize(id + 1);
			textures[id] = renderer->CreateTexture(img.data.get(), img.width, img.height, img.pf);
		}
		// replace image ids with texture ids
		for (auto& material : materials) {
			material.texAlbedo = textures[material.texAlbedo];
			// ...
		}
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
			renderer->EndUploadResources();
			loadContext = LoadContext();
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
				VertexType vertexType = VertexType::PN;
				MaterialIndex materialIndex = res.first->second;
				if (res.second) {
					materials.push_back({});
					Material material{};
					material.albedo = { surf.color[0], surf.color[1], surf.color[2] };
					material.metallic = surf.surface_infos[MeshLoader::SPECULARITY_MAP].val;
					material.roughness = surf.surface_infos[MeshLoader::GLOSSINESS_MAP].val;
					materials[materialIndex] = material;
					if (colLayers && colLayers->image && colLayers->image->path) {
						vertexType = VertexType::PNT;
						auto inserted = imageMap.insert({ s2ws(colLayers->image->path), (TextureIndex)images.size() });
						materials[materialIndex].texAlbedo = inserted.first->second;
						if (inserted.second) {
							images.push_back({});
							auto path = s2ws(colLayers->image->path);
							auto pos = path.rfind(L'\\');
							if (pos == std::wstring::npos) pos = 0;
							else ++pos;
							path = path.substr(pos);
#if defined(PLATFORM_WIN)
							imageLoadTasks.push_back(LoadImage(path.c_str(), inserted.first->second));
#elif defined(PLATFORM_MAC_OS)
							images[index] = LoadImage(path.c_str());
#endif
						}
					}
				}
				auto vOffset = (offset_t)vb.size(), iOffset = (offset_t)ib.size();
				l.submeshes.push_back({ materialIndex, vOffset, iOffset, 0/*stride*/, 0/*count*/, vertexType });
				if (vertexType == VertexType::PN) {
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
					vb.resize(vb.size() + AlignTo<decltype(vSize), 16>(vSize));
					ib.resize(ib.size() + iSize);
					memcpy(vb.data() + vOffset, pn.vertices.data(), vSize);
					memcpy(ib.data() + iOffset, pn.indices.data(), iSize);
				}
				else if (vertexType == VertexType::PNT) {
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
					vb.resize(vb.size() + AlignTo<decltype(vSize), 16>(vSize));
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
