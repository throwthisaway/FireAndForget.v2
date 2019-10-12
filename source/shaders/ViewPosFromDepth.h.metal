//
//#ifndef ViewPosFromDepth_h_h
//#define ViewPosFromDepth_h_h
//
//
//#endif /* ViewPosFromDepth_h_h */
#pragma once
float NDCDepthToViewDepth(float zNDC, float4x4 proj);
float3 ViewPosFromDepth(float depth, float3 pos, float4x4 proj);
