struct VS_P2UV {
	float2 pos : POSITION0;
	float2 uv : TEXCOORD0;
};
struct VS_PN {
	float3 pos : POSITION0;
	float3 n : NORMAL;
};
struct VS_PNUV {
	float3 pos : POSITION0;
	float3 n : NORMAL;
	float2 uv : TEXCOORD0;
};
struct VS_PUV {
	float3 pos : POSITION0;
	float2 uv : TEXCOORD0;
};

struct PS {
	float4 pos : SV_POSITION;
}; 
struct PS_UV {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

struct PS_P {
	float4 pos : SV_POSITION;
	float3 p : POSITION0;
};

struct PS_UVN {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 n : NORMAL0;
};

typedef struct PS_UVN GS_UVN;

struct PS_UVNT {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 n : NORMAL0;
	float3 t : TANGENT0;
};
