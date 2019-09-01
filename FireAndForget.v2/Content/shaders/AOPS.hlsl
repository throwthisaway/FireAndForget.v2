#include "ShaderInput.hlsli"
#include "ShaderStructs.h"

ConstantBuffer<AO> ao : register(b0);

float4 main() : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}