#ifndef VertexTypes_h
#define VertexTypes_h
struct VertexFSQuad {
	packed_float2 pos, uv;
};
struct VertexPN {
	packed_float3 pos, n;
};
struct VertexPNT {
	packed_float3 pos, n;
	packed_float2 uv0;
};
struct VertexPT {
	packed_float3 pos;
	packed_float2 uv0;
};
#endif /* VertexTypes_h */
