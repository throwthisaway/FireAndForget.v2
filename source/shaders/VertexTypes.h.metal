#include <metal_stdlib>
struct VPN {
	metal::packed_float3 pos, n;
};
struct VPNT {
	metal::packed_float3 pos, n;
	metal::packed_float2 uv0;
};
