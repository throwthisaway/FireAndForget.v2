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

struct GS_UVN {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 n : NORMAL0;
};

struct PS_UVNT {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 n : NORMAL0;
	float3 t : TANGENT0;
};
