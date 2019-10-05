#ifndef VertexTypes_h
#define VertexTypes_h
struct VertexFSQuad {
	packed_float2 pos, uv;
};
struct VertexPN {
	packed_float3 pos, n;
};
struct VertexPNUV {
	packed_float3 pos, n;
	packed_float2 uv0;
};
struct VertexPNTUV {
	packed_float3 pos, n, t;
	packed_float2 uv0;
};
struct VertexPT {
	packed_float3 pos;
	packed_float2 uv0;
};
#endif /* VertexTypes_h */
