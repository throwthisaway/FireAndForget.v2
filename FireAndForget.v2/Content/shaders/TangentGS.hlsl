#include "ShaderInput.hlsli"

[maxvertexcount(3)]
void main(triangle GS_PUVN input[3], 
	inout TriangleStream<PS_UVNT> output) {
	float3 e0 = (input[1].p - input[0].p).xyz;
	float3 e1 = (input[2].p - input[0].p).xyz;
	float2 duv0 = input[1].uv - input[0].uv;
	float2 duv1 = input[2].uv - input[0].uv;
	float f = 1.f / (duv0.x * duv1.y - duv0.y * duv1.x);
	float3 t = normalize(f * (duv1.y * e0 - duv0.y * e1));
	//float3 b = normalize(f * (duv0.x * e1 - duv1.x * e0));
	for (uint i = 0; i < 3; i++) {
		PS_UVNT element;
		element.pos = input[i].pos;
		element.uv = input[i].uv;
		element.nWS = input[i].nWS;
		element.nVS = input[i].nVS;
		element.tWS = t;
		output.Append(element);
	}
}