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
	float3 n : NORMAL0;
	float2 uv : TEXCOORD0;
};
struct VS_PNTUV {
	float3 pos : POSITION0;
	float3 n : NORMAL0;
	float3 t : TANGENT0;
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
struct PS_PUV {
	float4 pos : SV_POSITION;
	float3 pWS : POSITION0;
	float2 uv : TEXCOORD0;
};
struct PS_P {
	float4 pos : SV_POSITION;
	float3 p : POSITION0;
};
struct PS_PN {
	float4 pos : SV_POSITION;
	float3 pWS : POSITION0;
	float3 nWS : NORMAL0;
	float3 nVS : NORMAL1;
};
struct PS_PUVN {
	float4 pos : SV_POSITION;
	float3 pWS : POSITION0;
	float2 uv : TEXCOORD0;
	float3 nWS : NORMAL0;
	float3 nVS : NORMAL1;
};

struct PS_PUVNT {
	float4 pos : SV_POSITION;
	float3 pWS : POSITION0;
	float2 uv : TEXCOORD0;
	float3 nWS : NORMAL0;
	float3 nVS : NORMAL1;
	float3 tWS : TANGENT0;
};

struct PS_PUVNTVP {
	float4 pos : SV_POSITION;
	float3 pWS : POSITION0;
	float2 uv : TEXCOORD0;
	float3 nWS : NORMAL0;
	float3 nVS : NORMAL1;
	float3 tWS : TANGENT0;
	float3 vTS : VIEWTANGENT0;
	float3 pTS : POSTANGENT0;
};
