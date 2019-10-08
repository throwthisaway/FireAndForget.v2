#pragma once
#ifdef _MSC_VER
#define MSVC_ALIGN16 _declspec(align(16))
#define CLANG_ALIGN16
#else /* if CLANG */
#define CLANG_ALIGN16 __attribute__((aligned(16)))
#define MSVC_ALIGN16
#endif
#define row_major
#define column_major
// TODO:: replace everything if it works
#define ALIGN16 __attribute__((aligned(16)))
// TODO:: #define GLM_DEPTH_ZERO_TO_ONE
// TODO:: #define GLM_LEFT_HANDED
// TODO:: set GLM_CONFIG_CLIP_CONTROL to GLM_CLIP_CONTROL_LH_ZO
#include <glm/glm.hpp>
#ifdef PLATFORM_MAC_OS
#pragma clang diagnostic pop
#else
#endif
using float2 = CLANG_ALIGN16 MSVC_ALIGN16 glm::vec2;
using float3 = CLANG_ALIGN16 MSVC_ALIGN16 glm::vec3;
using float4 = CLANG_ALIGN16 MSVC_ALIGN16 glm::vec4;
using float4x4 = CLANG_ALIGN16 MSVC_ALIGN16 glm::mat4x4;
using float3x3 = CLANG_ALIGN16 MSVC_ALIGN16 glm::mat3x3;

using packed_float2 = glm::vec2;
using packed_float3 = glm::vec3;
