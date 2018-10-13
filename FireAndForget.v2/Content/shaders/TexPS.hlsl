#include "Common.hlsli"
#include "../../../source/cpp/ShaderStructs.h"

Texture2D<float4> tColor : register(t0);

ConstantBuffer<Material> mat : register(b0);

SamplerState smp : register(s0) {
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct PSIn {
	float4 pos : SV_POSITION;
	float3 n : NORMAL0;
	float2 uv0 : TEXCOORD0;
	float4 worldPos : POSITION0;
};

MRTOut main(PSIn input) {
	float4 diffuseColor = tColor.Sample(smp, input.uv0);
	MRTOut output;
	output.albedo = diffuseColor;
	output.normal = Encode(input.n);
	output.material = float4(mat.specular, mat.power, 0.f, 1.f);
	output.debug = float4(input.n, 1.f);
	return output;
}