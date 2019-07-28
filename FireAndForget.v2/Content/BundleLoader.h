#pragma once
#include <ppltasks.h>
namespace MeshLoader {
	struct Mesh;
}
concurrency::task<void> LoadFromBundle(Platform::String^ fname, MeshLoader::Mesh& mesh, std::function<void(bool)> completionCb);
