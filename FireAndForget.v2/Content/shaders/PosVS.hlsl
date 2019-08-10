#include "ShaderStructs.h"
ConstantBuffer<Object> obj : register(b0);

struct VIn {
	float3 pos : POSITION0;
	float3 n : NORMAL0;
};
struct PSIn {
	float4 pos : SV_POSITION;
	float4 worldPos : POSITION0;
	float3 n : NORMAL0;
};

PSIn main(VIn input) {
	PSIn output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(obj.mvp, pos);
	output.n = normalize(mul(obj.m, input.n));
	output.worldPos = mul(obj.m, pos);
	return output;
}