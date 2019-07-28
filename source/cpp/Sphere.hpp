#pragma once
#include <vector>
#include <glm/glm.hpp>

struct Sphere {
	std::vector<glm::vec3> vertices;
	Sphere(size_t depth);
private:
	void Subdivide(const glm::vec3& v1, const glm::vec3& v2,const glm::vec3& v3, size_t depth);
};
