#include "GenSSAOKernel.hpp"
#include <random>
namespace {
std::random_device rd;
std::mt19937 mt(rd());
}
std::array<glm::vec4, kKernelSize> GenSSAOKernel() {
	std::array<glm::vec4, kKernelSize> result;
	std::uniform_real_distribution dist(-1.f, 1.f);
	// http://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html
	for (int i = 0; i < kKernelSize; ++i) {
		glm::vec4 v = { dist(mt), dist(mt), (dist(mt)/* * .5f + .5f*/) /*0..1*/, 0.f/*pad*/};
		v = glm::normalize(v);
		float scale = (dist(mt) * .5f + .5f) * .75f + .25f; /* 0.25..1*/
		v = glm::mix(glm::vec4{}, v, scale * scale);
		result[i] = v;
	}
	return result;
}
