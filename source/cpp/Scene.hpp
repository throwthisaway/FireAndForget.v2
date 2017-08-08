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
		glm::vec3 pos, pivot, rot;
		float scale = 1.f;
		const Model& model;
		Materials::ColPosBuffer uniforms;
		Object(const Model& model) : model(model) {}
		void Update(double frame, double total);
	};
	void Init(RendererWrapper*);
	void Render(size_t encoderIndex);
	void Update(double frame, double total);
	Input input;
private:
	RendererWrapper* renderer_;

	Assets assets_;
	std::vector<Object> objects_;
	Camera camera_;
};
