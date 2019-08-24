#include "ShaderInput.hlsli"

[maxvertexcount(3)]
void main(triangle GS_UVN input[3], 
	inout TriangleStream<PS_UVNT> output) {
	for (uint i = 0; i < 3; i++) {
		PS_UVNT element;
		element.pos = input[i].pos;
		element.uv = input[i].uv;
		element.n = input[i].n;
		element.t = float3(0.f, 0.f, 0.f); // TODO::
		output.Append(element);
	}
}