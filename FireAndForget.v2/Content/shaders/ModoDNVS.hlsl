#include "ShaderInput.hlsli"
#include "ShaderStructs.h"

ConstantBuffer<Object> obj : register(b0);
#if 1
GS_PUVN main(VS_PNUV input) {
	GS_PUVN output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, obj.mvp);
	output.n = normalize(mul(input.n, obj.m));
	output.p = mul(input.pos, obj.m);
	output.uv = input.uv;
	return output;
}
#else
PS_UVNT main(VS_PNUV input) {
	PS_UVNT output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, obj.mvp);
	float3 n = normalize(mul(input.n, obj.m));
	output.n = n;
	output.uv = input.uv;
	float3 up = float3(0.f, 1.f, 0.f);
	if (n.y > .999f) up = float3(0.f, 0.f, 1.f);
	else if (n.y < -.999f) up = float3(0.f, 0.f, -1.f);
	float3 t = normalize(cross(n, up));
	output.t = t;
	return output;
}
#endif

