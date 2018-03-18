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
Assets::~Assets() = default;
#ifdef PLATFORM_WIN
Concurrency::task<void> Assets::LoadMesh(RendererWrapper* renderer, const wchar_t* fname, size_t id) {
	return DX::ReadDataAsync(fname).then([this, renderer, fname, id](std::vector<byte>& data) {
		MeshLoader::Mesh mesh;
		mesh.data = std::move(data);
		MeshLoader::LoadMesh(mesh.data.data(), mesh.data.size(), mesh);
		CreateModel(fname, renderer, staticModels[id], mesh);
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
#ifdef PLATFORM_WIN
	std::initializer_list<Concurrency::task<void>> loadMeshTasks{
		LoadMesh(renderer, L"light.mesh", LIGHT),
		LoadMesh(renderer, L"box.mesh", PLACEHOLDER),
		LoadMesh(renderer, L"checkerboard.mesh", CHECKERBOARD),
		LoadMesh(renderer, L"BEETHOVE_object.mesh", BEETHOVEN) };
	
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
void Assets::CreateModel(const wchar_t* name, RendererWrapper* renderer, Mesh& model, MeshLoader::Mesh& mesh) {
#ifdef INDEXED_DRAWING
	model.vb = renderer->CreateBuffer(mesh.vertices.data(), mesh.vertices.size() * sizeof(mesh.vertices[0]), sizeof(mesh.vertices[0]));
	model.ib = renderer->CreateBuffer(mesh.polygons.data(), mesh.polygons.size() * sizeof(mesh.polygons[0]), sizeof(mesh.polygons[0]));
	model.nb = renderer->CreateBuffer(mesh.normalsP.data(), mesh.normalsP.size() * sizeof(mesh.normalsP.front()), sizeof(mesh.normalsP.front()));
#else
	std::vector<vec3_t> vertices;
	vertices.reserve(mesh.vertices.size() * VERTICESPERPOLY);
	for (const auto& idx : mesh.polygons) {
		vertices.push_back(mesh.vertices[idx.v1]);
		vertices.push_back(mesh.vertices[idx.v2]);
		vertices.push_back(mesh.vertices[idx.v3]);
	}
	model.vb = renderer->CreateBuffer(vertices.data(), vertices.size() * sizeof(vertices[0]), sizeof(vertices[0]));
	model.nb = renderer->CreateBuffer(mesh.normalsPV.data(), mesh.normalsPV.size() * sizeof(mesh.normalsPV.front()), sizeof(mesh.normalsPV.front()) / VERTICESPERPOLY /*TODO:: fix this one normal contains 3 normals*/);
#endif
	model.colb = InvalidBuffer;
	// TODO:: uvmaps
	for (const auto& layer : mesh.layers) {
		model.layers.emplace_back(Mesh::Layer{});
		Mesh::Layer& modelLayer = model.layers.back();
		//submeshes
		for (size_t i = 0; i < layer.poly.count; ++i) {
			// Surface properties
			auto surfaceIndex = layer.poly.sections[i].index;
			auto surf = mesh.surfaces[surfaceIndex];
			auto colLayers = surf.surface_infos[MeshLoader::COLOR_MAP].layers;

			// material id: <object_name>_<surface_name>
			auto res = materials.emplace(std::wstring{ name } + L'_' + std::wstring{ s2ws(surf.name) }, Material{ });
			modelLayer.submeshes.push_back({ layer.poly.sections[i].offset * VERTICESPERPOLY, layer.poly.sections[i].count * VERTICESPERPOLY, res.first->second });
			/* currently, insertion always succeeds because of material id */
			if (res.second) {
				Material& gpuMaterial = res.first->second;
				{
					gpuMaterial.cColor = renderer->CreateShaderResource(sizeof(ShaderStructures::cColor), 1);
					ShaderStructures::cColor data;
					data.color[0] = surf.color[0]; data.color[1] = surf.color[1]; data.color[2] = surf.color[2]; data.color[3] = 1.f;// !!! surf.surface_infos[MeshLoader::TRANSPARENCY_MAP].val;
					// TODO:: !!!surf.surface_infos[MeshLoader::TRANSPARENCY_MAP].val; is 0.000
					renderer->UpdateShaderResource(gpuMaterial.cColor, &data, sizeof(ShaderStructures::cColor));
				}
				{
					gpuMaterial.cMaterial = renderer->CreateShaderResource(sizeof(ShaderStructures::cMaterial), 1);
					ShaderStructures::cMaterial data;
					data.material.diffuse[0] = surf.color[0]; data.material.diffuse[1] = surf.color[1]; data.material.diffuse[2] = surf.color[2];
					data.material.specular = surf.surface_infos[MeshLoader::SPECULARITY_MAP].val;
					data.material.power = surf.surface_infos[MeshLoader::GLOSSINESS_MAP].val; // TODO:: convert
					renderer->UpdateShaderResource(gpuMaterial.cMaterial, &data, sizeof(ShaderStructures::cMaterial));
				}
				if (// extract only the color info's first layer with image now
					colLayers && colLayers->image && colLayers->image->path) {
					const auto& uv = mesh.uvs.uvmaps[surfaceIndex][colLayers->uvmap];
					auto elementSize = UVCOORDS * sizeof(*uv.uv);
					gpuMaterial.staticColorUV = renderer->CreateBuffer(uv.uv, uv.n * elementSize * VERTICESPERPOLY, elementSize);
					auto path = s2ws(colLayers->image->path);
					if (!istrcmp(path, path.size() - 3, std::wstring{ L"tga" })) {
						path = extractFilename(path);
						auto res = images.emplace(path, Img::ImgData{});
						if (res.second) {
							Img::ImgData& color_image = res.first->second;
#ifdef PLATFORM_MAC_OS
							auto res = LoadFromBundle(path.c_str());
							// TODO::
#elif defined(PLATFORM_WIN)
							auto load = DX::ReadDataAsync(path).then([this, &gpuMaterial, &color_image, renderer](std::vector<byte>& data) {
								auto err = DecodeTGA(data.data(), data.size(), color_image, true);
								if (err != Img::TgaDecodeResult::Ok) {
									// TODO::
									assert(false);
							}
								else {
									gpuMaterial.tStaticColorTexture = renderer->CreateTexture(color_image.data.get(),
										color_image.width,
										color_image.height,
										color_image.bytesPerPixel,
										color_image.pf);
								}
						});
							loadTasks.push_back(load);
#endif
					}
				}
				}
			}
		}
	}
}
