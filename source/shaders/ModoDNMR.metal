#include "Common.h.metal"
#include "VertexTypes.h.metal"
#include "ShaderStructs.h"

// TODO:: declare sampler and texture
struct uObject {
	float4x4 mvp;
	float4x4 m;
};

struct FSIn {
	float4 pos [[position]];
	float3 it [[user(it)]],
		ib[[user(ib)]],
		in[[user(in)]];
	float3 n [[user(normal)]];
	float2 uv0 [[user(texcoord0)]];
};
float3 CalcTangent(float3 n) {
	float3 t(1.f, 0.f, 0.f);
	float d = dot(t, n);
	if (d < .001f) {
		t = float3(0.f, 1.f, 0.f);
		d = dot(t, n);
	}
	// Gram-Schmidt orthogonalize
	t = normalize(t - d * n);
	return t;

}
vertex FSIn modo_dnmr_vs_main(const device VertexPNTUV* input [[buffer(0)]],
						constant uObject& obj [[buffer(1)]],
						uint id [[vertex_id]]) {
	FSIn output;
	float4 pos = float4(input[id].pos, 1.f);
	output.pos = obj.mvp * pos;
	float3 n = float3x3(obj.m[0].xyz, obj.m[1].xyz, obj.m[2].xyz) * float3(input[id].n);
	float3 t = CalcTangent(n);
	float3 b = cross(t, n);
	float3x3 tbn(t, b, n);
	output.in = float3(n.x, t.x, b.x);
	output.it = float3(n.y, t.y, b.y);
	output.ib = float3(n.z, t.z, b.z);
	output.n = n;
	output.uv0 = input[id].uv0;
	return output;
}
fragment FragOut modo_dnmr_fs_main(FSIn input [[stage_in]],
							 array<texture2d<float>, 4> textures [[texture(0)]],
							 sampler smp [[sampler(0)]]) {
	FragOut output;
	output.albedo = textures[0].sample(smp, input.uv0);
	float3 n = textures[1].sample(smp, input.uv0).xyz * 2.f - 1.f;
	n = n * float3x3(normalize(input.it), normalize(input.ib), normalize(input.in));
	output.normal = Encode(n);
	float metallic = textures[2].sample(smp, input.uv0).x;
	float roughness = textures[3].sample(smp, input.uv0).x;
	output.material = float4(metallic, roughness, 0.f, 1.f);
	output.debug = float4(n, 1.f);
	return output;
}
