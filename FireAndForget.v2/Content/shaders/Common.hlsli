static const float M_PI_F = 3.14159265f;

struct MRTOut {
	float4 albedo : SV_TARGET0;
	float4 normalWS : SV_TARGET1;
	float4 normalVS : SV_TARGET2;
	float4 material : SV_TARGET3;
#ifdef DEBUG_RT
	float4 debug : SV_TARGET4;
#endif
};

// as seen on https://aras-p.info/texts/CompactNormalStorage.html
// Method #4
// This one is better but still has nans
half2 Encode(half3 n)
{
    half2 enc = normalize(n.xy) * (sqrt(-n.z*0.5+0.5));
    enc = enc*0.5+0.5;
    return enc;
}
half3 Decode(half2 enc)
{
    half4 nn = half4(enc, 0, 0)*half4(2,2,0,0) + half4(-1,-1,1,-1);
    half l = dot(nn.xyz,-nn.xyw);
    nn.z = l;
    nn.xy *= sqrt(l);
    return nn.xyz * 2 + half3(0,0,-1);
}
//float2 Encode(float3 v) {
//	// does not work well for z = -1.f => v.xy / 0 = nan
//	float p = sqrt(v.z * 8.f + 8.f);
//	return float2(v.xy / p + .5f);
//}
//
//float3 Decode(float2 v) {
//	float2 fenc = v * 4.f - 2.f;
//	float f = dot(fenc, fenc);
//	float g = sqrt(1.f - f / 4.f);
//	float3 n;
//	n.xy = fenc * g;
//	n.z = 1.f - f / 2.f;
//	return n;
//}

float LinearizeDepth(float depth, float n, float f) {
	return (2.0 * n) / (f + n - depth * (f - n));
}

float3 GammaCorrection(float3 c) {
	const float gamma = 2.2f, invExp = 1.f/gamma;
	c = c / (c + (float3)1.f);
	return pow(c, invExp);
}