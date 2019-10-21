#pragma once
#include "../../ModoMeshLoader/ModoMeshLoader.h"
struct ModoMesh {
	std::string name;
	float r;
	AABB aabb;
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer;
	std::vector<ModoMeshLoader::Submesh> submeshes;
};
