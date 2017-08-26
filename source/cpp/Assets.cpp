#include "pch.h"
#include "compatibility.h"
#include "Assets.hpp"
#include <assert.h>
#ifdef PLATFORM_MAC_OS
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFStream.h>
#include <CoreFoundation/CFNumber.h>
#elif defined(PLATFORM_WIN)
#include "Content\BundleLoader.h"
#endif
#include "MeshLoader.h"
#include "FileReader.h"
namespace {
#ifdef PLATFORM_MAC_OS
	void LoadFromBundle(const char* fname) {
		CFBundleRef mainBundle;
		mainBundle = CFBundleGetMainBundle();
		assert(mainBundle);

		CFURLRef url = CFBundleCopyResourceURL(mainBundle,  CFSTR("BEETHOVE_object.mesh"), NULL, NULL);
		//	CFUrlRef --> const char*
		//	CFStringRef pathURL = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
		//
		//	CFStringEncoding encodingMethod = CFStringGetSystemEncoding();
		//	const char *path = CFStringGetCStringPtr(pathURL, encodingMethod);
		//	CFRelease(pathURL);
		//	CFRelease(url);

		CFErrorRef error;
		CFNumberRef fileSizeRef;
		signed long long int fileSize = 0;
		if (CFURLCopyResourcePropertyForKey(url, kCFURLFileSizeKey, &fileSizeRef, &error) && fileSizeRef) {
			CFNumberGetValue(fileSizeRef, kCFNumberLongLongType, &fileSize);
			CFRelease(fileSizeRef);
		}
		if (!fileSize) {
			CFRelease(url);
			CFRelease(error);
			return;
		}
		CFReadStreamRef rs = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
		Boolean res = CFReadStreamOpen(rs);
		if (res) {
			std::unique_ptr<UInt8[]> buffer(new UInt8[fileSize]);
			CFIndex read = CFReadStreamRead(rs, (UInt8*)buffer.get(), fileSize);
			if (read == fileSize) {
				MeshLoader::Mesh mesh;
				LoadMesh(buffer.get(), fileSize, mesh);
			} else {
				// error read != filesize
			}
			CFReadStreamClose(rs);
		} else {
			// cannot open stream
		}
		CFRelease(rs);
		CFRelease(url);
		CFRelease(error);
		// ???
		CFRelease(mainBundle);
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
	struct VertexPositionColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 color;
	};
	VertexPositionColor cubeVertices[] =
	{
		{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
	};

	const UINT vertexBufferSize = sizeof(cubeVertices);
	auto posCol = renderer->CreateBuffer(cubeVertices, vertexBufferSize, sizeof(VertexPositionColor));

	unsigned short cubeIndices[] =
	{
		0, 2, 1, // -x
		1, 2, 3,

		4, 5, 6, // +x
		5, 7, 6,

		0, 1, 5, // -y
		0, 5, 4,

		2, 6, 7, // +y
		2, 7, 3,

		0, 4, 6, // -z
		0, 6, 2,

		1, 3, 7, // +z
		1, 7, 5,
	};

	const UINT indexBufferSize = sizeof(cubeIndices);
	auto index = renderer->CreateBuffer(cubeIndices, indexBufferSize, sizeof(unsigned short));

	staticModels[PLACEHOLDER1] = {posCol, 0, index,  0, indexBufferSize / sizeof(unsigned short) };

	VertexPositionColor cubeVertices2[] =
	{
		{ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.5f, -0.5f,  0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.5f,  0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(1.5f,  0.5f,  0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
	};
	auto posCol2 = renderer->CreateBuffer(cubeVertices2, vertexBufferSize, sizeof(VertexPositionColor));
	staticModels[PLACEHOLDER2] = { posCol2, 0, index, 0, indexBufferSize / sizeof(unsigned short) };
#ifdef PLATFORM_WIN
	LoadFromBundle("checkerboard.mesh", mesh, [this, renderer](bool success) {
		auto vertices = renderer->CreateBuffer(mesh.vertices.data(), mesh.vertices.size() * sizeof(mesh.vertices[0]), sizeof(mesh.vertices[0]));
		// TODO:: if (!mesh.polygons.empty()
		auto indices = renderer->CreateBuffer(mesh.polygons.data(), mesh.polygons.size() * sizeof(mesh.polygons[0]), sizeof(mesh.polygons[0]));
		for (const auto& layer : mesh.layers) {
			for (size_t i = 0; i < layer.poly.count; ++i) {
				staticModels[CHECKERBOARD] = { vertices, 0, indices, layer.poly.sections[i].offset, layer.poly.sections[i].count * VERTICESPERPOLY };
				break;
				// TODO::
			}
			break;
			// TODO::
		}
		renderer->EndUploadResources();
		loadCompleted = true;
	});
#endif
}
