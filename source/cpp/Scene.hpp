#pragma once
#include "compatibility.h"
#include <glm/glm.hpp>
#include "ShaderStructures.h"
#include "Assets.hpp"
#include "input/Input.hpp"
#include "Camera.hpp"
#ifdef PLATFORM_WIN
#include "../Content/ShaderStructures.h"
#include "../Content/Renderer.h"
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
		ShaderResourceIndex cScene, cAO;
	};

	struct Object {
		vec3_t pos, rot;
		index_t mesh;
		void Update(double frame, double total);
	};
	void Init(Renderer*, int, int);
	void PrepareScene();
	void PrepareScene();
	void Render();
	void Update(double frame, double total);
	void UpdateCameraTransform();
	void UpdateSceneTransform();
	Input input;
	enum class State{Start, AssetsLoading, Ready};
	State state = State::Start;
private:
	glm::mat4 m;	// for scene transform
	Transform transform;
	struct {
		int width, height;
	}viewport_;
	Camera camera_;
	struct Light {
		ShaderStructures::PointLight pointLight;
		index_t placeholder;
	}lights_[MAX_LIGHTS];
	Renderer* renderer_;
	assets::Assets assets_;
	std::vector<Object> objects_;
	struct {
		ShaderStructures::cScene cScene;
		ShaderStructures::cAO cAO;
	}shaderStructures;
	SceneShaderResources shaderResources;
	TextureIndex cubeEnv_ = InvalidTexture;
	ShaderStructures::DeferredBuffers deferredBuffers_;
};
