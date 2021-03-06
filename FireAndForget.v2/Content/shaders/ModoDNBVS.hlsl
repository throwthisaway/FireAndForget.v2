#include "ShaderInput.hlsli"
#include "ShaderStructs.hlsli"

ConstantBuffer<Object> obj : register(b0);
ConstantBuffer<SceneCB> scene : register(b1);

PS_PUVNTVP main(VS_PNTUV input) {
	PS_PUVNTVP output;
	float4 pos = float4(input.pos, 1.f);
	output.pos = mul(pos, obj.mvp);
	output.nWS = normalize(mul(input.n, obj.m));
	output.tWS = normalize(mul(input.t, obj.m));
	output.nVS = normalize(mul(input.n, obj.mv));
	output.uv = input.uv;
	output.pWS = mul(pos, obj.m).xyz;
	float3 bWS = cross(output.nWS, output.tWS);
	float3x3 itbn = transpose(float3x3(output.tWS, bWS, output.nWS));
	output.vTS = mul(scene.eyePos, itbn);
	output.pTS = mul(output.pWS, itbn);
	return output;
}
