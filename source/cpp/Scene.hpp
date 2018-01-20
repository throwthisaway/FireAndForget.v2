#pragma once
#include "compatibility.h"
#include <glm/glm.hpp>
#include "RendererWrapper.h"
#include "ShaderStructures.h"
#include "Assets.hpp"
#include "input/Input.hpp"
#include "Camera.hpp"
#ifdef PLATFORM_WIN
#include "../Content/ShaderStructures.h"
#endif // PLATFORM_WIN

struct Time;

struct Scene {
	struct SceneShaderResources {
		ShaderResourceIndex cScene;
	};
	struct Object {
		glm::vec3 pos, rot;
		float scale = 1.f;
		
		struct Layer {
			glm::vec3 pivot;
			// pos
			ShaderResourceIndex cMVP;
			std::vector<ShaderStructures::PosCmd> posCmd;
			// tex
			ShaderResourceIndex cObject;
			std::vector<ShaderStructures::TexCmd> texCmd;
		};
		std::vector<Layer> layers;
	
		Object(RendererWrapper* renderer, const Mesh& mesh, const SceneShaderResources& sceneShaderResources);
		void Update(double frame, double total);
	};
	void Init(RendererWrapper*, int, int);
	void Render();
	void Update(double frame, double total);
	Input input;
	bool loadCompleted = false;
private:
	RendererWrapper* renderer_;
	Assets assets_;
	std::vector<Object> objects_;
	struct {
		ShaderStructures::cScene cScene;
	}shaderStructures;
	SceneShaderResources shaderResources;
	Camera camera_;

	// TODO:: remove
	float	m_radiansPerSecond;
	float	m_angle;
	bool	m_tracking;
	// TODO:: remove
};
