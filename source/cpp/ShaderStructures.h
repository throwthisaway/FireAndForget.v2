#pragma once

namespace ShaderStructures {
	using Id = size_t;
const Id ColPos = 0;
const Id Pos = 1;
const Id Tex = 2;
const Id Count = 3;

// basic hlsl type aliases
using float4 = float[4];
using float3 = float[3];
using matrix = float[16];

// template order corresponds root parameter order of root signature
template<typename... T> struct pack {};
template<typename... T> struct ShaderParamTraits { using types = pack<T...>; };

// Pos
struct cMVP {
	matrix mvp;
};
struct cColor {
	float4 color;
};

using PosVSParams = ShaderParamTraits<cMVP>;
using PosPSParams = ShaderParamTraits<cColor>;

// Tex
struct cObjectVS {
	matrix mvp;
	matrix m;
};

struct tDiffuse {};
struct Material {
	float3 diffuse;
	float specular, power;
};
struct cObjectPS {
	Material material;
};

struct PointLight {
	float4 diffuse;
	float4 ambient;
	float4 specular;
	float3 pos;
	float3 att;
	float range;
};
#define MAX_LIGHTS 2
struct cFrame {
	PointLight light[MAX_LIGHTS];
	float4 eyePos;
};
using TexVSShaderParams = ShaderParamTraits<cObjectVS>;
using TexPSShaderParams = ShaderParamTraits<tDiffuse, cObjectPS, cFrame>;

struct cBuffers {
	using buf_size_t = uint16_t;
	std::vector<std::pair<const uint8_t*, buf_size_t>> data;
};

struct ConstantColorRef : cBuffers {
	ConstantColorRef(const cMVP& mvp, const cColor& color) {
		assert(sizeof(mvp) < std::numeric_limits<buf_size_t>::max());
		data.emplace_back(reinterpret_cast<const uint8_t*>(&mvp), (buf_size_t)sizeof(mvp));
		assert(sizeof(color) < std::numeric_limits<buf_size_t>::max());
		data.emplace_back(reinterpret_cast<const uint8_t*>(&color), (buf_size_t)sizeof(color));
	}
};

}
