#pragma once
#include "compatibility.h"
#include <glm/glm.hpp>
#include "ShaderStructures.h"
#include "Assets.hpp"
#include "input/Input.hpp"
#include "Camera.hpp"
#ifdef PLATFORM_WIN
#include "../Content/Renderer.h"
#elif defined(PLATFORM_MAC_OS)
#include "../Renderer.h"
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
	struct Object {
		float3 pos, rot;
		index_t mesh;
		void Update(double frame, double total);
	};
	void Init(Renderer*, int, int);
	void PrepareScene();
	void Render();
	void Update(double frame, double total);
	void UpdateCameraTransform();
	void UpdateSceneTransform();
	void OnResize(int width, int height);
	Input input;
	enum class State{Start, AssetsLoading, Ready};
	State state = State::Start;
private:
	void ObjectsWindow();
	void SceneWindow();
	struct {
		bool cameraOpen = true, aoOpen = true, lightOpen = true;
	} ui;
	glm::mat4 m;	// for scene transform
	Transform transform;
	struct {
		int width, height;
	}viewport_;
	Camera camera_;
	struct Light {
		//PointLight pointLight;
		index_t placeholder;
	}lights_[MAX_LIGHTS];
	float r = 0.f;
	AABB aabb;
	struct ShadowMap {
		float4x4 p;
		float3 dir;
		float dim;
		RTIndex rt;
		float factor;
	}shadowMaps_[MAX_SHADOWMAPS];

	struct Projector {
		float4x4 p;
		float3 from, to;
		float fov;
		TextureIndex tex;
		float factor;
	}projectors_[MAX_PROJECTORS];
	Renderer* renderer_;
	assets::Assets assets_;
	std::vector<Object> objects_;
	std::vector<Object> modoObjects_;
	TextureIndex cubeEnv_ = InvalidTexture;
	ShaderStructures::DeferredCmd deferredCmd_;
	ShaderStructures::SSAOCmd ssaoCmd_;
	bool prepared_ = false;
};
