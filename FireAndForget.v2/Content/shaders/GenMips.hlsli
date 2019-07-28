#include "ColorSpaceUtility.hlsli"
//https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli
//https://www.3dgep.com/learning-directx-12-4/#Compute_Shader
#define RootSig \
    "RootFlags(0), " \
    "DescriptorTable(CBV(b0, numDescriptors = 1)), " \
    "DescriptorTable(SRV(t0, numDescriptors = 1))," \
    "DescriptorTable(UAV(u0, numDescriptors = 4))," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
	"filter = FILTER_MIN_MAG_MIP_LINEAR)"

cbuffer cb : register(b0)  {
	uint srcMipLevel;   
	uint numMipLevels;
	float2 texelSize;    // 1. / dim
};

RWTexture2D<float4> out1 : register(u0);
RWTexture2D<float4> out2 : register(u1);
RWTexture2D<float4> out3 : register(u2);
RWTexture2D<float4> out4 : register(u3);
Texture2D<float4> src : register(t0);
SamplerState smp : register(s0);

#ifndef NPOT
#define NPOT 0
#endif

#ifdef SRGB
#define CONVERT(linear) float4(ApplySRGBCurve_Fast(linear.rgb), linear.a)
#else
#define CONVERT(linear) linear
#endif

#define NUMTHREADS 8
groupshared float gs_R[NUMTHREADS * NUMTHREADS];
groupshared float gs_G[NUMTHREADS * NUMTHREADS];
groupshared float gs_B[NUMTHREADS * NUMTHREADS];
groupshared float gs_A[NUMTHREADS * NUMTHREADS];

void Store(uint tid, float4 color) {
	gs_R[tid] = color.r;
	gs_G[tid] = color.g;
	gs_B[tid] = color.b;
	gs_A[tid] = color.a;
}
float4 Load(uint tid) {
	return float4(gs_R[tid], gs_G[tid], gs_B[tid], gs_A[tid]);
}

[RootSignature(RootSig)]
[numthreads(NUMTHREADS, NUMTHREADS, 1)]
void main(uint tid : SV_GroupIndex, uint3 dtid : SV_DispatchThreadID) {

#if NPOT == 0
	float4 color = src.SampleLevel(smp, (float2(dtid.xy) + .5f) * texelSize, srcMipLevel);
#elif NPOT == 1
	// x - odd
	float4 color = (src.SampleLevel(smp, (dtid.xy + float2(.25f, .5f)) * texelSize, srcMipLevel)
				  + src.SampleLevel(smp, (dtid.xy + float2(.75f, .5f)) * texelSize, srcMipLevel)) * .5f;
#elif NPOT == 2
	// y - odd
	float4 color = (src.SampleLevel(smp, (dtid.xy + float2(.5f, .25f)) * texelSize, srcMipLevel)
				  + src.SampleLevel(smp, (dtid.xy + float2(.5f, .75f)) * texelSize, srcMipLevel)) * .5f;
#elif NPOT == 3
	// x - odd, y - odd
	float4 color = (src.SampleLevel(smp, (dtid.xy + float2(.25f, .25f)) * texelSize, srcMipLevel)
				  + src.SampleLevel(smp, (dtid.xy + float2(.25f, .75f)) * texelSize, srcMipLevel)
				  + src.SampleLevel(smp, (dtid.xy + float2(.75f, .25f)) * texelSize, srcMipLevel)
				  + src.SampleLevel(smp, (dtid.xy + float2(.75f, .75f)) * texelSize, srcMipLevel)) * .25f;
#endif
	out1[dtid.xy] = CONVERT(color);
	if (numMipLevels == 1) return;
	Store(tid, color);
	GroupMemoryBarrierWithGroupSync();
	
	// even threads
	if (!(tid & 0x9/*0b001001*/)) {
		color = (color + Load(tid + 0x1) + Load(tid + 0x8) + Load(tid + 0x9)) * .25f;
		out2[dtid.xy >> 1] = CONVERT(color);
		Store(tid, color);
	}
	if (numMipLevels == 2) return;
	GroupMemoryBarrierWithGroupSync();

	// multiple of four threads
	if (!(tid & 0x1b/*0b011011*/)) {
		color = (color + Load(tid + 0x2) + Load(tid + 0x10) + Load(tid + 0x12)) * .25f;
		out2[dtid.xy >> 2] = CONVERT(color);
		Store(tid, color);
	}
	if (numMipLevels == 3) return;
	GroupMemoryBarrierWithGroupSync();

	// multiple of eight threads
	if (!tid) {
		color = (color + Load(tid + 0x4) + Load(tid + 0x20) + Load(tid + 0x24)) * .25f;
		out2[dtid.xy >> 3] = CONVERT(color);
		Store(tid, color);
	}
}
