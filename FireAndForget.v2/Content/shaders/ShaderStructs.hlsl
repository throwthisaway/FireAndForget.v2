struct PointLight {
	float3 diffuse;
	float3 ambient;
	float3 specular;
	float3 pos;
	float3 att;
	float range;
};
struct Material {
	float3 diffuse;
	float specular, power;
};
