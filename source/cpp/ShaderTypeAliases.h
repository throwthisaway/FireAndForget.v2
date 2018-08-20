#pragma once
#ifdef _MSC_VER
#define ALIGN16 _declspec(align(16))
#else
#define ALIGN16
#endif
using float4 = ALIGN16 float[4];
using float3 = ALIGN16 float[3];
using float4x4 = ALIGN16 float[16];
