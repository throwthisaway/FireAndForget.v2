#pragma once
#include <vector>
#include "RendererTypes.h"
#include "compatibility.h"
#include "ShaderTypeAliases.h"

namespace ShaderStructures {
#include "ShaderStructs.h"
const int FrameCount = 3;
using ShaderId = size_t;
const ShaderId Pos = 0;
const ShaderId Tex = 1;
const ShaderId Debug = 2;
const ShaderId Deferred = 3;
const ShaderId Count = 4;
const int RenderTargetCount = 4;

// resource types
template<int Count>
struct cDesc { static constexpr int desc_count = Count; };
template<int Count = 1>
struct cFrame : cDesc<Count> { static constexpr int frame_count = FrameCount; };
template<int Count = 1>
struct cStatic : cDesc<Count> { static constexpr int frame_count = 1; };
template<int Count = 1>
struct cTexture : cDesc<Count> { static constexpr int frame_count = 1; };

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
struct desc_count<TCurrent, TRest...> : std::integral_constant<uint32_t, TCurrent::desc_count + desc_count<TRest...>::value> {};

// template order corresponds root parameter order of root signature
template<typename... T> struct ShaderParamTraits {
	template<typename S> using index = index<S, T...>;
	static constexpr auto desc_count = desc_count<T...>::value;
	//using numDesc = desc_count<T...>;
	static constexpr uint16_t count = sizeof...(T);
};

#ifdef PLATFORM_WIN
// root parameter/attrib index -> resource
struct ResourceBinding {
	uint32_t paramIndex;	// root parameter or attrib index
	uint16_t offset;	// one resource per frame
};
#elif defined(PLATFORM_MAC_OS)
struct BufferInfo {
	BufferIndex bufferIndex;	// root parameter or attrib index
	uint16_t bufferCount;	// one resource per frame
};
#endif
// Debug
struct cMVP : cFrame<> {
	float4x4 mvp;
};
struct cColor : cStatic<> {
	float4 color;
};

struct DebugCmd {
	uint32_t offset, count;
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer;
#ifdef PLATFORM_WIN
	DescAllocEntryIndex descAllocEntryIndex; // to determine descriptorheap
	using Params = ShaderParamTraits<cMVP, cColor>;
	static constexpr int RootParamCount = 2;
	ResourceBinding bindings[RootParamCount];
#elif defined(PLATFORM_MAC_OS)
	using VSParams = ShaderParamTraits<cMVP>;
	using FSParams = ShaderParamTraits<cColor>;
	BufferInfo vsBuffers[VSParams::count], fsBuffers[FSParams::count];
#endif
};

// Pos-Tex
struct cObject : cFrame<> {
	Object obj;
};

struct cMaterial : cStatic<> {
	Material material;
};

struct cScene : cFrame<> {
	Scene scene;
};
// Pos

struct PosCmd {
	uint32_t offset, count;
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer, nb = InvalidBuffer;
#ifdef PLATFORM_WIN
	DescAllocEntryIndex descAllocEntryIndex; // to determine descriptorheap
	using Params = ShaderParamTraits<cObject, cMaterial>;
	static constexpr int RootParamCount = 2;
	ResourceBinding bindings[RootParamCount];
#elif defined(PLATFORM_MAC_OS)
	using VSParams = ShaderParamTraits<cObject>;
	using FSParams = ShaderParamTraits<cMaterial>;
	BufferInfo vsBuffers[VSParams::count], fsBuffers[FSParams::count];
#endif
};
// Tex
template<int Count>
struct tTexture : cTexture<Count> {};

struct TexCmd {
	uint32_t offset, count;
	BufferIndex vb = InvalidBuffer, ib = InvalidBuffer, nb = InvalidBuffer, uvb = InvalidBuffer;
#ifdef PLATFORM_WIN
	DescAllocEntryIndex descAllocEntryIndex; // to determine descriptorheap
	using Params = ShaderParamTraits<cObject, tTexture<1>, cMaterial>;
	static constexpr int RootParamCount = 2;
	ResourceBinding bindings[RootParamCount];
#elif defined(PLATFORM_MAC_OS)
	using VSParams = ShaderParamTraits<cObject>;
	using FSParams = ShaderParamTraits<cMaterial>;
	BufferInfo vsBuffers[VSParams::count], fsBuffers[FSParams::count];
	TextureIndex textures[1];
#endif
};

struct DeferredBuffers {
#ifdef PLATFORM_WIN
	DescAllocEntryIndex descAllocEntryIndex; // to determine descriptorheap
	using Params = ShaderParamTraits<cScene, tTexture<RenderTargetCount + 1 /* Depth */>>;
	static constexpr int RootParamCount = 1;
	ResourceBinding bindings[RootParamCount];
#elif defined(PLATFORM_MAC_OS)
	using VSParams = ShaderParamTraits<>;
	using FSParams = ShaderParamTraits<cScene>;
	BufferInfo vsBuffers[VSParams::count], fsBuffers[FSParams::count];
#endif
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
