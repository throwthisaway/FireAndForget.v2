struct PS {
	float4 pos : SV_POSITION;
}; 
struct PS_T {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

struct PS_P {
	float4 pos : SV_POSITION;
	float3 p : POSITION0;
};

struct PS_TN {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 n : NORMAL0;
};

struct PS_TN_TNB {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 n : NORMAL0;
	float3x3 tnb : TNB0;
};
