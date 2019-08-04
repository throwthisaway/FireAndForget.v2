
struct VIn {
	float3 pos : POSITION0;
	float3 n : NORMAL0;
};

cbuffer cMVP : register(b0) {
	float4x4 mvp;
};

float4 main(VIn input) : SV_POSITION {
	return mul(mvp, float4(input.pos, 1.0f));
}
