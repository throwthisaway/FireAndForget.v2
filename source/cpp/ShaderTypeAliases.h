#pragma once
#ifdef _MSC_VER
#define MSVC_ALIGN16 _declspec(align(16))
#define CLANG_ALIGN16
#else
#define CLANG_ALIGN16 __attribute__((aligned(16)))
#define MSVC_ALIGN16
#endif
using float2 = MSVC_ALIGN16 glm::vec2;
using float3 = MSVC_ALIGN16 glm::vec3;
using float4 = MSVC_ALIGN16 glm::vec4;
using float4x4 = MSVC_ALIGN16 glm::mat4x4;
