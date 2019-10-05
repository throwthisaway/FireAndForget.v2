#include "ModoMeshLoader.h"
#include <assert.h>
namespace ModoMeshLoader {
	namespace {
		std::vector<uint8_t> LoadData(const uint8_t* p, uint32_t size) {
			std::vector<uint8_t> result;
			result.resize(size);
			memcpy(result.data(), p, size);
			return result;
		}
		std::vector<Submesh> LoadSubmeshes(const uint8_t* p, uint32_t size, uint32_t count) {
			std::vector<Submesh> result;
			for (int j = 0; j < count; ++j) {
				Submesh submesh;
				submesh.indexByteOffset = *(uint32_t*)p; p+=sizeof(uint32_t);
				submesh.vertexByteOffset = *(uint32_t*)p; p+=sizeof(uint32_t);
				submesh.stride = *(uint32_t*)p; p+=sizeof(uint32_t);
				submesh.material.diffuse.r = *(float*)p; p+=sizeof(float);
				submesh.material.diffuse.g = *(float*)p; p+=sizeof(float);
				submesh.material.diffuse.b = *(float*)p; p+=sizeof(float);
				submesh.material.metallic_roughness.r = *(float*)p; p+=sizeof(float);
				submesh.material.metallic_roughness.g = *(float*)p; p+=sizeof(float);
				submesh.textureMask = *(uint32_t*)p; p+=sizeof(uint32_t);
				submesh.uvCount = *(uint32_t*)p; p+=sizeof(uint32_t);
				for (int i = 0; i < (int)TextureTypes::kCount; ++i) {
					submesh.textures[i].id = *(uint32_t*)p; p+=sizeof(uint32_t);
					submesh.textures[i].uv = *(uint32_t*)p; p+=sizeof(uint32_t);
				}
				submesh.count = *(uint32_t*)p; p+=sizeof(uint32_t);
				result.push_back(submesh);
			}
			return result;
		}
		std::vector<std::string> LoadImages(const uint8_t* p, uint32_t size, uint32_t count) {
			std::vector<std::string> result;
			auto q = p;
			while (q - p  < size) {
			size_t len = strlen((const char*)q);
				result.push_back({(const char*)q, len}); ++q;
				q += len;
			}
			return result;
		}
	}
	Result Load(const std::vector<uint8_t>& data) {
		Result result;
		size_t offset = 0;
		auto p = data.data();
		auto end = data.data() + data.size();
		while (p < end) {
			uint32_t tag = *(uint32_t*)p; p+=sizeof(tag);
			uint32_t size = *(uint32_t*)p; p+=sizeof(size);
			uint32_t count = *(uint32_t*)p; p+=sizeof(count);
			if (tag == Tag(POLY))
				result.indices = LoadData(p, size);
			else if (tag == Tag(VERT))
				result.vertices = LoadData(p, size);
			else if (tag == Tag(MATR))
				result.submeshes = LoadSubmeshes(p, size, count);
			else if (tag == Tag(IMAG))
				result.images = LoadImages(p, size, count);
			else
				assert(false);
			p+= size;
			offset += size;
		}
		return result;
	}
}
