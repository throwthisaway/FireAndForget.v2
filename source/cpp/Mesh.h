#pragma once
enum class VertexType{PUV, PN, PT, PNT};
struct Material {
	vec3_t albedo;
	float metallic, roughness;
	TextureIndex texAlbedo;
};
struct Submesh {
	MaterialIndex material;
	offset_t vbByteOffset, ibByteOffset, stride;
	index_t count;
	VertexType vertexType;
};
struct Layer {
	vec3_t pivot;
	std::vector<Submesh> submeshes;
};
struct Mesh {
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer;
	std::vector<Layer> layers;
};
