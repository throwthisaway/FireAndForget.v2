struct VIn {
	float2 pos : POSITION0;
	float2 uv0 : TEXCOORD0;
};

struct PSIn {
	float4 pos : SV_POSITION;
	float2 uv0 : TEXCOORD0;
};

PSIn main(VIn input) {
	PSIn output;
	output.pos = float4(input.pos, 0.f, 1.f);
	output.uv0 = input.uv0;
	return output;
}