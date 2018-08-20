#include "../../../source/cpp/ShaderStructs.h"
cbuffer cObject : register(b0) {
	Object obj;
};

struct VIn {
	float3 pos : POSITION0;
	float3 n : NORMAL0;
	float2 uv0 : TEXCOORD0;
};
struct PSIn {
	float4 pos : SV_POSITION;
	float3 n : NORMAL0;
	float2 uv0 : TEXCOORD0;
	float4 worldPos : POSITION0;
};

PSIn main(VIn input) {
	PSIn output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, obj.mvp);
	output.n = normalize(mul(input.n, obj.m));
	output.uv0 = input.uv0;
	output.worldPos = mul(pos, obj.m);
	return output;
}