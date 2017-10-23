#pragma once
#include <glm/glm.hpp>

namespace Material {
	using Id = size_t;
const Id ColPos = 0;
const Id Pos = 1;
const Id Count = 2;

struct ColPosBuffer {
	glm::mat4 mvp;
};
struct cBuffers {
	using buf_size_t = uint16_t;
	std::vector<std::pair<const uint8_t*, buf_size_t>> data;
};

struct cMVP {
	glm::mat4 mvp;
};
struct cColor {
	glm::vec4 color;
};
struct ConstantColorRef : cBuffers {
	ConstantColorRef(const cMVP& mvp, const cColor& color) {
		assert(sizeof(mvp) < std::numeric_limits<buf_size_t>::max());
		data.emplace_back(reinterpret_cast<const uint8_t*>(&mvp), (buf_size_t)sizeof(mvp));
		assert(sizeof(color) < std::numeric_limits<buf_size_t>::max());
		data.emplace_back(reinterpret_cast<const uint8_t*>(&color), (buf_size_t)sizeof(color));
	}
};

}
