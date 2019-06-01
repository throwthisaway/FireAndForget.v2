struct VertexFSQuad {
	float2 pos : POSITION0;
	float2 uv : TEXCOORD0;
};
struct VertexPN {
	float3 pos : POSITION0;
	float3 n : NORMAL;
};
struct VertexPNT {
	float3 pos : POSITION0;
	float3 n : NORMAL;
	float2 uv : TEXCOORD0;
};
struct VertexPT {
	float3 pos : POSITION0;
	float2 uv : TEXCOORD0;
};