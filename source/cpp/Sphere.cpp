#include "Sphere.hpp"

#define ARRAYSIZE(x) sizeof(x) / sizeof(x[0])

static float radius/*R*/ = 0.295f,
X = 0.525731112119133606f,
Z = 0.850650808352039932f;
static const glm::vec3 vdata[] = {{ -X, 0.0, Z },
    { X, 0.0, Z },
    { -X, 0.0, -Z },
    { X, 0.0, -Z },
    { 0.0, Z , X },
    { 0.0, Z , -X  },
    { 0.0 , -Z , X  },
    { 0.0 , -Z , -X },
    { Z , X, 0.0 },
    { -Z , X, 0.0  },
    { Z , -X, 0.0  },
	{ -Z , -X, 0.0 }};
static const size_t	tindices[][3] =  {{0, 4, 1}, { 0, 9, 4 }, { 9, 5, 4 }, { 4, 5, 8 }, { 4, 8, 1 },
				 { 8, 10, 1 }, { 8, 3, 10 }, { 5, 3, 8 }, { 5, 2, 3 }, { 2, 7, 3 },
				 { 7, 10, 3 }, { 7, 6, 10 }, { 7, 11, 6 }, { 11, 0, 6 }, { 0, 1, 6 },
				 { 6, 1, 10 }, { 9, 0, 11 }, { 9, 11, 2 }, { 9, 2, 5 }, { 7, 2, 11 }};

Sphere::Sphere(size_t depth) {
	vertices.reserve(depth * sizeof(tindices) * 3 * 3);
	for(auto i = 0u; i < ARRAYSIZE(tindices); ++i) {
		Subdivide(vdata[tindices[i][0]], vdata[tindices[i][1]], vdata[tindices[i][2]], depth);
	}
}

void Sphere::Subdivide(const glm::vec3& v1, const glm::vec3& v2,const glm::vec3& v3, size_t depth) {
	if (depth <= 0) {
		vertices.push_back(v1);
		vertices.push_back(v2);
		vertices.push_back(v3);
		return;
	}
	const glm::vec3 v12 = glm::normalize(v1 + v2),
		v23 = glm::normalize(v2 + v3),
		v31 = glm::normalize(v3 + v1);
	Subdivide(v1, v12, v31,  depth - 1);
	Subdivide(v2, v23, v12,  depth - 1);
	Subdivide(v3, v31, v23,  depth - 1);
	Subdivide(v12, v23, v31,  depth - 1);
}
