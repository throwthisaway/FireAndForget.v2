#include "Common.hlsli"
cbuffer cColor : register(b0){
	float4 color;
};

MRTOut main(float4 pos : SV_POSITION) {
	MRTOut output;
	output.albedo = color;
	output.normalWS = float4(normalize((float3)1.f), 1.f);
	output.normalVS = float4(normalize((float3)1.f), 1.f);
	output.material = float4(0.f, 0.f, 0.f, 1.f);
	output.positionWS = float4(0.f, 0.f, 0.f, 0.f);
	return output;
}
