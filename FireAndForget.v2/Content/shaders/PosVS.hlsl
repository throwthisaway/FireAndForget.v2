cbuffer cObject : register(b0) {
	matrix mvp;
	matrix m;
};

struct VIn {
	float3 pos : POSITION0;
	float3 n : NORMAL0;
};
struct PSIn {
	float4 pos : SV_POSITION;
	float3 n : NORMAL0;
};

PSIn main(VIn input) {
	PSIn output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, mvp);
	output.n = normalize(mul(input.n, m));
	return output;
}