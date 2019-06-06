struct PS {
	float4 pos : SV_POSITION;
}; 
struct PS_T {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

struct PS_P {
	float4 pos : SV_POSITION;
	float3 p; // localpos
}; 