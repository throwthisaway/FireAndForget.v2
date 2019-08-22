#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "SIMDTypeAliases.h"
#include "Platform.h"
#include "ShaderStructs.h"

#define MATR "MATR"
#define POLY "POLY"
#define VERT "VERT"
#define IMAG "IMAG"

namespace ModoMeshLoader {
	using tag_t = uint32_t;
	using index_t = uint16_t;	// Index buffer element type
	inline const int kVertPerPoly = 3;
	enum class TextureTypes{kAlbedo, kNormal, kMetallic, kRoughness, kCount};
	struct Texture {
		unsigned id, uv;
	};
	struct Submesh {
		//string name;
		uint32_t indexByteOffset, vertexByteOffset, stride;
		Material material;
		unsigned textureMask;
		unsigned uvCount;
		Texture textures[(int)TextureTypes::kCount];
		uint16_t count;
	};

	constexpr tag_t Tag(const char* t) {
		return (t[3] << 24) | (t[2] << 16) | (t[1] << 8) | t[0];
	}

	struct Result {
		std::vector<std::string> images;
		std::vector<Submesh> submeshes;
		std::vector<uint8_t> vertices, indices;
	};
	Result Load(const std::vector<uint8_t>& data);
}
