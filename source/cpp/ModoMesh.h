#pragma once
#include "../../ModoMeshLoader/ModoMeshLoader.h"
struct ModoMesh {
	std::string name;
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer;
	std::vector<ModoMeshLoader::Submesh> submeshes;
};
