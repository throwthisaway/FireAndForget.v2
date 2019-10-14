#pragma once
#define row_major
#define column_major
#define ALIGN16 alignas(16)
// TODO:: #define GLM_DEPTH_ZERO_TO_ONE
// TODO:: #define GLM_LEFT_HANDED
// TODO:: set GLM_CONFIG_CLIP_CONTROL to GLM_CLIP_CONTROL_LH_ZO
#include <glm/glm.hpp>
#ifdef PLATFORM_MAC_OS
#pragma clang diagnostic pop
#else
#endif
using float2 = glm::vec2;
using float3 = glm::vec3;
using float4 = glm::vec4;
using float4x4 = glm::mat4x4;
using float3x3 = glm::mat3x3;

using packed_float2 = glm::vec2;
using packed_float3 = glm::vec3;
