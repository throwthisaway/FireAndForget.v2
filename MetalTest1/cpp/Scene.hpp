#pragma once
#include <glm/glm.hpp>
#include "RendererWrapper.h"
#include "Assets.hpp"
#include "input/Input.hpp"
#include "Materials.h"
#include "Camera.hpp"

struct Time;

struct Scene {
	struct Object {
		glm::mat4 m;
		glm::vec4 pos, rot;
		float scale;
		size_t assetId;
	};
	void Init(RendererWrapper*);
	void Render();
	void Update(double frame, double total);
	Input input;
private:
	RendererWrapper* renderer_;
	
	Materials::ColPosBuffer uniforms;
	Assets assets_;
	std::vector<Object> objects_;
	Camera camera_;
};
