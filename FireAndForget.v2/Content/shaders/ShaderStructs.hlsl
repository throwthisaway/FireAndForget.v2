struct PointLight {
	float3 diffuse;
	float pad1;
	float3 ambient;
	float pad2;
	float3 specular;
	float pad3;
	float3 pos;
	float pad4;
	float3 att;
	float pad5;
	float range;
	float3 pad6;
};
struct Material {
	float3 diffuse;
	float pad1;
	float specular, power;
};