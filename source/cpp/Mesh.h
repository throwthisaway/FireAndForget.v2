#pragma once
enum class VertexType{PUV, PN, PT, PNT};

struct Submesh {
	MaterialIndex material;
	TextureIndex texAlbedo;
	offset_t vbByteOffset, ibByteOffset, stride;
	index_t count;
	VertexType vertexType;
};
struct Layer {
	float3 pivot;
	std::vector<Submesh> submeshes;
};
struct Mesh {
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer;
	std::vector<Layer> layers;
};
