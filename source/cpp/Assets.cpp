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
void Assets::Init(RendererWrapper* renderer) {
	renderer->BeginUploadResources();
	//LoadFromBundle("dssdsds");
	//static const float positions[] =
	//{
	//	0.0,  5., 0, 1,
	//	-5., -5., 0, 1,
	//	5., -5., 0, 1,
	//};

	//static const float colors[] =
	//{
	//	1, 0, 0, 1,
	//	0, 1, 0, 1,
	//	0, 0, 1, 1,
	//};
	//auto pos = renderer->CreateBuffer(positions, sizeof(positions) * sizeof(positions[0]), sizeof(float) * 4);
	//auto col = renderer->CreateBuffer(colors, sizeof(colors) * sizeof(colors[0]), sizeof(float) * 4);
	//staticModels[PLACEHOLDER1] = {pos, col, 0, sizeof(positions) / sizeof(positions[0]) / 4};
	//static const float positions2[] =
	//{
	//	0.5,  0.5, 0, 1,
	//	0., -0.5, 0, 1,
	//	1., -0.5, 0, 1,
	//};
	//auto pos2 = renderer->CreateBuffer(positions2, sizeof(positions2) * sizeof(positions2[0]), sizeof(float) * 4);
	//staticModels[PLACEHOLDER2] = {pos2, col, 0, sizeof(positions2) / sizeof(positions[0]) / 4};
#ifdef PLATFORM_WIN
	//auto res = materials.emplace(L"default", Material{ { 1.f, 1.f, 1.f },
	//	InvalidBuffer,
	//	InvalidBuffer,
	//	1.f, 128.f, 1.f });
	//auto& defaultMaterial = res.first->second;
	//struct VertexPositionColor
	//{
	//	DirectX::XMFLOAT3 pos;
	//	DirectX::XMFLOAT3 color;
	//};
	//VertexPositionColor cubeVertices[] =
	//{
	//	{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
	//	{ XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
	//	{ XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
	//	{ XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
	//	{ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
	//	{ XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
	//	{ XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
	//	{ XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
	//};

	//const UINT vertexBufferSize = sizeof(cubeVertices);
	//auto posCol = renderer->CreateBuffer(cubeVertices, vertexBufferSize, sizeof(VertexPositionColor));

	//unsigned short cubeIndices[] =
	//{
	//	0, 2, 1, // -x
	//	1, 2, 3,

	//	4, 5, 6, // +x
	//	5, 7, 6,

	//	0, 1, 5, // -y
	//	0, 5, 4,

	//	2, 6, 7, // +y
	//	2, 7, 3,

	//	0, 4, 6, // -z
	//	0, 6, 2,

	//	1, 3, 7, // +z
	//	1, 7, 5,
	//};

	//const UINT indexBufferSize = sizeof(cubeIndices);
	//auto index = renderer->CreateBuffer(cubeIndices, indexBufferSize, sizeof(unsigned short));

	//staticModels[PLACEHOLDER1] = { posCol, InvalidBuffer, index, InvalidBuffer, {{{/*pivot*/}, {{ 0, indexBufferSize / sizeof(unsigned short), defaultMaterial }}}} };

	//VertexPositionColor cubeVertices2[] =
	//{
	//	{ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
	//	{ XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
	//	{ XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
	//	{ XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
	//	{ XMFLOAT3(1.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
	//	{ XMFLOAT3(1.5f, -0.5f,  0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
	//	{ XMFLOAT3(1.5f,  0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
	//	{ XMFLOAT3(1.5f,  0.5f,  0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
	//};
	//auto posCol2 = renderer->CreateBuffer(cubeVertices2, vertexBufferSize, sizeof(VertexPositionColor));
	//staticModels[PLACEHOLDER2] = { posCol2, InvalidBuffer, index, InvalidBuffer, { { {/*pivot*/ },{ { 0, indexBufferSize / sizeof(unsigned short), defaultMaterial } } } } };

	auto checkerboardTask = DX::ReadDataAsync(L"checkerboard.mesh").then([this, renderer](std::vector<byte>& data) {
		MeshLoader::Mesh mesh;
		mesh.data = std::move(data);
		MeshLoader::LoadMesh(mesh.data.data(), mesh.data.size(), mesh);
		CreateModel(L"checkerboard.mesh", renderer, staticModels[CHECKERBOARD], mesh);
	});
	auto beethovenTask = DX::ReadDataAsync(L"BEETHOVE_object.mesh").then([this, renderer](std::vector<byte>& data) {
		MeshLoader::Mesh mesh;
		mesh.data = std::move(data);
		MeshLoader::LoadMesh(mesh.data.data(), mesh.data.size(), mesh);
		CreateModel(L"BEETHOVE_object.mesh", renderer, staticModels[BEETHOVEN], mesh);
	});
	loadCompleteTask = (checkerboardTask && beethovenTask).then([this, renderer]() {
		return Concurrency::when_all(std::begin(loadTasks), std::end(loadTasks)).then([this, renderer]() {
			loadTasks.clear();
			renderer->EndUploadResources();
		}); });
#elif defined(PLATFORM_MAC_OS)
	{
		auto data = LoadFromBundle(L"checkerboard.mesh");
		MeshLoader::Mesh mesh;
		mesh.data = std::move(data);
		MeshLoader::LoadMesh(mesh.data.data(), mesh.data.size(), mesh);
		CreateModel(L"checkerboard.mesh", renderer, staticModels[CHECKERBOARD], mesh);
	}
	{
		auto data = LoadFromBundle(L"BEETHOVE_object.mesh");
		MeshLoader::Mesh mesh;
		mesh.data = std::move(data);
		MeshLoader::LoadMesh(mesh.data.data(), mesh.data.size(), mesh);
		CreateModel(L"BEETHOVE_object.mesh", renderer, staticModels[BEETHOVEN], mesh);
	}
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
	// TODO:: if (!mesh.polygons.empty()
#ifdef PLATFORM_WIN
	//for (auto& p : mesh.polygons) {
	//	auto& temp = const_cast<MeshLoader::Polygon&>(p);
	//	std::swap(temp.v1, temp.v3);
	//}
#endif
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
					data.color[0] = surf.color[0]; data.color[1] = surf.color[1]; data.color[2] = surf.color[2]; data.color[3] = surf.surface_infos[MeshLoader::TRANSPARENCY_MAP].val;
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
