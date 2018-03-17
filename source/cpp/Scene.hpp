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

// TODO::
// - root param index count is not equal to this anymore: auto rootParamIndex = ShaderStructures::TexParams::index<ShaderStructures::tTexture>::value;
// - load cso in separate tasks, reuse them if possible
// - refactor Renderer::Submit commands: share code
// - debug draw should have separate command list, with depth test turned off
// - check is PosParams and PosCmd (TexParams and TexCmd, ...) can be linked
// - rewrite shaders to 5.x
// - implicit truncation of vector type: 	output.n = normalize(mul(input.n, m));
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
			// debug
			ShaderResourceIndex cMVP;
			std::vector<ShaderStructures::DebugCmd> debugCmd;
			// pos-tex
			ShaderResourceIndex cObject;
			//pos
			std::vector<ShaderStructures::PosCmd> posCmd;
			// tex
			std::vector<ShaderStructures::TexCmd> texCmd;
		};
		std::vector<Layer> layers;
	
		Object(RendererWrapper* renderer, const Mesh& mesh, const SceneShaderResources& sceneShaderResources, bool debug);
		void Update(double frame, double total);
	};
	void Init(RendererWrapper*, int, int);
	void Render();
	void Update(double frame, double total);
	void UpdateCameraTransform();
	void UpdateSceneTransform();
	Input input;
	bool loadCompleted = false;
private:
	Transform transform;
	Camera camera_;
	RendererWrapper* renderer_;
	Assets assets_;
	std::vector<Object> objects_;
	struct {
		ShaderStructures::cScene cScene;
	}shaderStructures;
	SceneShaderResources shaderResources;

	// TODO:: remove
	float	m_radiansPerSecond;
	float	m_angle;
	bool	m_tracking;
	// TODO:: remove
};
