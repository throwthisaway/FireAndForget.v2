#pragma once
#include <array>
#include <glm/glm.hpp>

constexpr int kKernelSize = 14;
std::array<glm::vec4, kKernelSize> GenSSAOKernel();
