#pragma once
#include "RendererTypes.h"
#include "compatibility.h"

namespace ShaderStructures {
#ifdef PLATFORM_MAC_OS
	const int FrameCount = 1;
#else
	const int FrameCount = 3;
#endif
	using ShaderId = size_t;
const ShaderId ColPos = 0;
const ShaderId Pos = 1;
const ShaderId Tex = 2;
const ShaderId Count = 3;

// basic hlsl type aliases
using float4 = float[4];
using float3 = float[3];
using matrix = float[16];

// resource types
struct cFrame { static constexpr int numDesc = ShaderStructures::FrameCount; };
struct cStatic { static constexpr int numDesc = 1; };
struct cTexture { static constexpr int numDesc = 1; };

// https://stackoverflow.com/questions/30736242/how-can-i-get-the-index-of-a-type-in-a-variadic-class-template
template <typename... > struct index;

// found it
template <typename T, typename... TRest>
struct index<T, T, TRest...> : std::integral_constant<uint32_t, 0> {};

// still looking
template <typename T, typename TCurrent, typename... TRest>
struct index<T, TCurrent, TRest...> : std::integral_constant<uint32_t, 1 + index<T, TRest...>::value> {};


template <typename... > struct desc_count;
template <>
struct desc_count<> : std::integral_constant<uint32_t, 0> {};
template <typename TCurrent, typename... TRest>
struct desc_count<TCurrent, TRest...> : std::integral_constant<uint32_t, TCurrent::numDesc + desc_count<TRest...>::value> {};

// template order corresponds root parameter order of root signature
template<typename... T> struct ShaderParamTraits {
	template<typename S> using index = index<S, T...>;
	using numDesc = desc_count<T...>;
	static constexpr uint16_t count = sizeof...(T);
};

// root parameter/attrib index -> resource
struct ResourceBinding {
	uint32_t paramIndex;	// root parameter or attrib index
	uint16_t offset, count;	// one resource per frame, count number of frames
};

// Pos
struct cMVP : cFrame {
	matrix mvp;
};
struct cColor : cStatic {
	float4 color;
};
using PosParams = ShaderParamTraits<cMVP, cColor>;

struct PosCmd {
	uint32_t offset, count;
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer;
	DescAllocEntryIndex descAllocEntryIndex; // to determine descriptorheap
	ResourceBinding bindings[PosParams::count];
};

//using PosVSParams = ShaderParamTraits<cMVP>;
//using PosPSParams = ShaderParamTraits<cStaticColor>;

// Tex
struct cObject : cFrame {
	matrix mvp;
	matrix m;
};

struct tTexture : cTexture {};
struct Material {
	float3 diffuse;
	float specular, power;
};
struct cMaterial : cStatic {
	Material material;
};

struct PointLight {
	float3 diffuse;
	float3 ambient;
	float3 specular;
	float3 pos;
	float3 att;
	float range;
};
#define MAX_LIGHTS 2
struct cScene : cFrame {
	PointLight light[MAX_LIGHTS];
	float3 eyePos;
};
using TexParams = ShaderParamTraits<cObject, tTexture, cMaterial, cScene>;

struct TexCmd {
	uint32_t offset, count;
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer, nb = InvalidBuffer, uvb = InvalidBuffer;
	DescAllocEntryIndex descAllocEntryIndex; // to determine descriptorheap
	ResourceBinding bindings[TexParams::count];
};
//using TexVSSParams = ShaderParamTraits<cObjectVS>;
//using TexPSParams = ShaderParamTraits<tStaticTexture, cObjectPS, cFrame>;
//

//
//struct DynamicResource {
//	ShaderResourceIndex resourceStartIndex;	// one resource per frame
//	void* ptr;
//	uint32_t size;
//};

struct cBuffers {
	using buf_size_t = uint16_t;
	std::vector<std::pair<const uint8_t*, buf_size_t>> data;
};

struct ConstantColorRef : cBuffers {
	ConstantColorRef(const cMVP& mvp, const cColor& color) {
#undef max
		assert(sizeof(mvp) < std::numeric_limits<buf_size_t>::max());
		data.emplace_back(reinterpret_cast<const uint8_t*>(&mvp), (buf_size_t)sizeof(mvp));
		assert(sizeof(color) < std::numeric_limits<buf_size_t>::max());
		data.emplace_back(reinterpret_cast<const uint8_t*>(&color), (buf_size_t)sizeof(color));
	}
};

}
