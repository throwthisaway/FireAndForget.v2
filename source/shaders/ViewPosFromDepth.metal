#include <metal_stdlib>
using namespace metal;

// from: https://github.com/d3dcoder/d3d12book/blob/master/Chapter%2021%20Ambient%20Occlusion/Ssao/Shaders/Ssao.hlsl
float NDCDepthToViewDepth(float zNDC, float4x4 proj) {
	// z_ndc = A + B/viewZ, where proj[2,2]=A and proj[3,2]=B.
	// [3][2] => [2][3] due scene.proj is row_major
	// [3][2] is not adressing [3].z instead into the kernel array for some reason
	float p23 = proj[3].z, p22 = proj[2].z;
	return p23 / (zNDC - p22);
}
float3 ViewPosFromDepth(float depth, float3 pos, float4x4 proj) {
	float z = NDCDepthToViewDepth(depth, proj);
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pos.
	// p.z = t*pos.z
	// t = p.z / pos.z
	return (z / pos.z) * pos;
}

