#pragma once
#ifdef PLATFORM_WIN
using float4 = float[4];
using float3 = float[3];
using float4x4 = float[16];
#elif defined(PALTFORM_MAC_OS)
// TODO:: simd_* types
#endif