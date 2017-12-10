cbuffer cObject : register(b0) {
	matrix mvp;
	matrix m;
};

struct VIn {
	float3 pos : POSITION0;
	float3 n : NORMAL0;
	float2 uv0 : TEXCOORD0;
};
struct PSIn {
	float4 pos : SV_POSITION;
	float4 world_pos : POSITION0;
	float3 n : NORMAL0;
	float2 uv0 : TEXCOORD0;
};

PSIn main(VIn input) {
	PSIn output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(mvp, pos);
	output.world_pos = mul(m, pos);
	output.n = normalize(mul(m, input.n));
	output.uv0 = input.uv0;

	return output;
}