#pragma once
#include "ModoMeshLoader.h"
struct ModoMesh {
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer;
	std::vector<ModoMeshLoader::Submesh> submeshes;
};
