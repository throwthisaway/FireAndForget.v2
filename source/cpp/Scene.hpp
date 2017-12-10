#pragma once
#include "compatibility.h"
#include <glm/glm.hpp>
#include "RendererWrapper.h"
#include "Assets.hpp"
#include "input/Input.hpp"
#include "Materials.h"
#include "Camera.hpp"
#ifdef PLATFORM_WIN
#include "../Content/ShaderStructures.h"
#endif // PLATFORM_WIN

struct Time;

struct Scene {
	struct Object {
		glm::mat4 m;
		glm::vec4 color;
		glm::vec3 pos, pivot, rot;
		float scale = 1.f;
		const Mesh& mesh;
		struct {
			GPUMaterials::Id id;
			size_t mvpStartIndex, colorStartIndex;
		}material;
#ifdef PLATFORM_WIN
		//FireAndForget_v2::ModelViewProjectionConstantBuffer perVertexColor;
#else
		Materials::ColPosBuffer uniforms;
#endif
		Object(const Mesh& mesh) : mesh(mesh) {}
		void Update(double frame, double total);
	};
	void Init(RendererWrapper*, int, int);
	void Render(size_t encoderIndex);
	void Update(double frame, double total);
	Input input;
private:
	RendererWrapper* renderer_;

	Assets assets_;
	std::vector<Object> objects_;
	Camera camera_;

	// TODO:: remove
	float	m_radiansPerSecond;
	float	m_angle;
	bool	m_tracking;
	// TODO:: remove
};
