#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "RendererTypes.h"
#include "compatibility.h"
#include "ShaderTypeAliases.h"
#include "Mesh.h"

namespace ShaderStructures {
#include "ShaderStructs.h"
const int FrameCount = 3;
const ShaderId Pos = 0;
const ShaderId Tex = 1;
const ShaderId Debug = 2;
const ShaderId Deferred = 3;
const ShaderId DeferredPBR = 4;
const ShaderId CubeEnvMap = 5;
const ShaderId Bg = 6;
const ShaderId Irradiance = 7;
const ShaderId PrefilterEnv = 8;
const ShaderId BRDFLUT = 9;
const ShaderId Downsample = 10;
const ShaderId GenMips = 11; // POT, Linear
const ShaderId GenMipsOddX = 12; // Linear
const ShaderId GenMipsOddY = 13; // Linear
const ShaderId GenMipsOddXOddY = 14; // Linear
const ShaderId GenMipsSRGB = 15; // POT, SRGB
const ShaderId GenMipsOddXSRGB = 16; // SRGB
const ShaderId GenMipsOddYSRGB = 17; // SRGB
const ShaderId GenMipsOddXOddYSRGB = 18; // SRGB
const ShaderId Count = 19;
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
	static constexpr auto desc_count = ShaderStructures::desc_count<T...>::value;
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

struct DrawCmd {
	const float4x4& m, mvp;
	const Submesh& submesh;
	const Material& material;
	BufferIndex vb, ib;
	ShaderId shader;
};
struct BgCmd {
	const float4x4& vp;
	const Submesh& submesh;
	BufferIndex vb, ib;
	ShaderId shader;
	TextureIndex cubeEnv;
};

struct DeferredCmd {
	Scene scene;
	AO ao;
	TextureIndex irradiance = InvalidTexture,
		prefilteredEnvMap = InvalidTexture,
		BRDFLUT = InvalidTexture,
		random = InvalidTexture;
};

}
