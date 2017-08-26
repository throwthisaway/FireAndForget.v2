// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer cMVP : register(b0) {
	matrix mvp;
};

float4 main(float3 pos : POSITION) : SV_POSITION {
	return mul(float4(pos, 1.0f), mvp);
}
