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
			result.resize(count);
			memcpy(result.data(), p, size);
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
		auto p = data.data();
		auto end = data.data() + data.size();
		while (p < data.data() + data.size()) {
			uint32_t tag = *(uint32_t*)p; p+=sizeof(tag);
			uint32_t size = *(uint32_t*)p; p+=sizeof(tag);
			uint32_t count = *(uint32_t*)p; p+=sizeof(tag);
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
		}
		return result;
	}
}
