#pragma once
#ifdef _MSC_VER
#define ALIGN16 _declspec(align(16))
#else
#define ALIGN16
#endif
using float3 = ALIGN16 glm::vec3;
using float4 = ALIGN16 glm::vec4;
using float4x4 = ALIGN16 glm::mat4x4;
