#include "pch.h"
#include "compatibility.h"
#include "Assets.hpp"
#include <assert.h>
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
#include "meshoptimizer.h"
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
}
namespace assets {
	Assets::~Assets() = default;
#ifdef PLATFORM_WIN
	Concurrency::task<void> Assets::LoadMesh(RendererWrapper* renderer, const wchar_t* fname, size_t id) {
		return DX::ReadDataAsync(fname).then([this, renderer, fname, id](std::vector<byte>& data) {
			MeshLoader::Mesh mesh;
			mesh.data = std::move(data);
			MeshLoader::LoadMesh(mesh.data.data(), mesh.data.size(), mesh);
			models[id] = CreateModel(fname, renderer, mesh);
		});
	}
#elif defined(PLATFORM_MAC_OS)
	void Assets::LoadMesh(RendererWrapper* renderer, const wchar_t* fname, size_t id) {
		auto data = LoadFromBundle(fname);
		MeshLoader::Mesh mesh;
		mesh.data = std::move(data);
		MeshLoader::LoadMesh(mesh.data.data(), mesh.data.size(), mesh);
		CreateModel(fname, renderer, staticModels[id], mesh);
	}
#endif
	void Assets::Init(RendererWrapper* renderer) {
		renderer->BeginUploadResources();
		models.resize(STATIC_MODEL_COUNT);
#ifdef PLATFORM_WIN
		std::initializer_list<Concurrency::task<void>> loadMeshTasks{
			LoadMesh(renderer, L"light.mesh", LIGHT),
			LoadMesh(renderer, L"box.mesh", PLACEHOLDER),
			LoadMesh(renderer, L"checkerboard.mesh", CHECKERBOARD),
			LoadMesh(renderer, L"BEETHOVE_object.mesh", BEETHOVEN),
			LoadMesh(renderer, L"sphere.mesh", SPHERE), };

		loadCompleteTask = Concurrency::when_all(std::begin(loadMeshTasks), std::end(loadMeshTasks)).then([this, renderer]() {
			return Concurrency::when_all(std::begin(loadTasks), std::end(loadTasks)).then([this, renderer]() {
				loadTasks.clear();
				renderer->EndUploadResources();
			}); });
#elif defined(PLATFORM_MAC_OS)
		LoadMesh(renderer, L"light.mesh", LIGHT);
		LoadMesh(renderer, L"box.mesh", PLACEHOLDER);
		LoadMesh(renderer, L"checkerboard.mesh", CHECKERBOARD);
		LoadMesh(renderer, L"BEETHOVE_object.mesh", BEETHOVEN);
		LoadMesh(renderer, L"sphere.mesh", SPHERE);
		renderer->EndUploadResources();
#endif
	}
	//SubMesh ConvertSectionToSubmesh(RendererWrapper* renderer, const MeshLoader::Layer::Sections::Section& section) {
	//	SubMesh result{ section.offset * VERTICESPERPOLY, section.count * VERTICESPERPOLY };
	//	CreateShaderResources
	//	return result;
	//}
	//Mesh::Layer ConvertLayer(RendererWrapper* renderer, const MeshLoader::Layer& layer) {
	//	Mesh::Layer result;
	//	for (size_t i = 0; i < layer.poly.count; ++i)
	//		result.submeshes.push_back((ConvertSectionToSubmesh(renderer, layer.poly.sections[i])));
	//	return result;
	//}
	bool DecodeImage(const std::wstring& path, std::vector<uint8_t>& data, Img::ImgData& image) {
		if (!istrcmp(path, path.size() - 3, std::wstring{ L"tga" })) {
			auto err = Img::DecodeTGA(data.data(), data.size(), image, true);
			if (err != Img::TgaDecodeResult::Ok) {
				// TODO::
				assert(false);
				return false;
			}
		}
		else {
			assert(false);	// TODO:: more file formats
			return false;
		}
		return true;
	}
	void Assets::LoadFromBundle(const char * path, const ImageLoadCB& cb) {
		auto fname = extractFilename(s2ws(path));
		auto res = images.emplace(fname, Img::ImgData{});
		if (!res.second) return;
		Img::ImgData& image = res.first->second;
#ifdef PLATFORM_MAC_OS
		auto data = ::LoadFromBundle(fname.c_str());
		bool success = DecodeImage(fname, data, image);
		cb(success, image);
#elif defined(PLATFORM_WIN)
		auto load = DX::ReadDataAsync(fname).then([fname, cb, &image](std::vector<uint8_t>& data) {
			bool success = DecodeImage(fname, data, image);
			cb(success, image);
		});
		loadTasks.push_back(load);
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
	Mesh Assets::CreateModel(const wchar_t* name, RendererWrapper* renderer, MeshLoader::Mesh& mesh) {
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

				auto res = materialMap.emplace(std::wstring{ name } + L'_' + std::wstring{ s2ws(surf.name) }, (MaterialIndex)materials.size());
				ShaderId id = InvalidShader;
				MaterialIndex materialIndex = res.first->second;
				if (res.second) {
					id = ShaderStructures::Pos;
					Material material;
					material.albedo = { surf.color[0], surf.color[1], surf.color[2] };
					material.metallic = surf.surface_infos[MeshLoader::SPECULARITY_MAP].val;
					material.roughness = surf.surface_infos[MeshLoader::GLOSSINESS_MAP].val;
					materials.push_back(material);
					if (colLayers && colLayers->image && colLayers->image->path) {
						id = ShaderStructures::Tex;
						LoadFromBundle(colLayers->image->path, [this, renderer, materialIndex](bool success, Img::ImgData& image) {
							assert(success);
							if (success) materials[materialIndex].texAlbedo = renderer->CreateTexture(image.data.get(),
								image.width,
								image.height,
								image.bytesPerPixel,
								image.pf);
						});
					}
				}
				auto vOffset = (offset_t)vb.size(), iOffset = (offset_t)ib.size();
				l.submeshes.push_back({ materialIndex, vOffset, iOffset, 0/*stride*/, 0/*count*/, id });
				if (id == ShaderStructures::Pos) {
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
					vb.resize(vb.size() + vSize);
					ib.resize(ib.size() + iSize);
					memcpy(vb.data() + vOffset, pn.vertices.data(), vSize);
					memcpy(ib.data() + iOffset, pn.indices.data(), iSize);
				}
				else if (id == ShaderStructures::Tex) {
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
					vb.resize(vb.size() + vSize);
					ib.resize(ib.size() + iSize);
					memcpy(vb.data() + vOffset, pnt.vertices.data(), vSize);
					memcpy(ib.data() + iOffset, pnt.indices.data(), iSize);
				}
			}
		}

		result.vb = renderer->CreateBuffer(vb.data(), vb.size());
		result.ib = renderer->CreateBuffer(ib.data(), ib.size());
		return result;
	}
}