#include "SIMDTypeAliases.h"
#pragma once
#ifdef _MSC_VER
#define MSVC_ALIGN16 _declspec(align(16))
#define CLANG_ALIGN16
#else
#define CLANG_ALIGN16 __attribute__((aligned(16)))
#define MSVC_ALIGN16
#endif
using float2 = MSVC_ALIGN16 vec2_t;
using float3 = MSVC_ALIGN16 vec3_t;
using float4 = MSVC_ALIGN16 vec4_t;
using float4x4 = MSVC_ALIGN16 mat4x4_t;

using packed_float2 = vec2_t;
using packed_float3 = vec3_t;
