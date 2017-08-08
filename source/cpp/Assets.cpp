#include "Assets.hpp"
#include <assert.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFStream.h>
#include <CoreFoundation/CFNumber.h>
#include "MeshLoader.h"
#include "FileReader.h"
namespace {
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
}

void Assets::Init(RendererWrapper* renderer) {
	LoadFromBundle("dssdsds");
	static const float positions[] =
	{
		0.0,  5., 0, 1,
		-5., -5., 0, 1,
		5., -5., 0, 1,
	};

	static const float colors[] =
	{
		1, 0, 0, 1,
		0, 1, 0, 1,
		0, 0, 1, 1,
	};
	auto pos = renderer->CreateBuffer(positions, sizeof(positions) * sizeof(positions[0]), sizeof(float) * 4);
	auto col = renderer->CreateBuffer(colors, sizeof(colors) * sizeof(colors[0]), sizeof(float) * 4);
	staticModels[PLACEHOLDER1] = {pos, col, sizeof(positions) / sizeof(positions[0]) / 4};
	static const float positions2[] =
	{
		0.5,  0.5, 0, 1,
		0., -0.5, 0, 1,
		1., -0.5, 0, 1,
	};
	auto pos2 = renderer->CreateBuffer(positions2, sizeof(positions2) * sizeof(positions2[0]), sizeof(float) * 4);
	staticModels[PLACEHOLDER2] = {pos2, col, sizeof(positions2) / sizeof(positions[0]) / 4};
}
