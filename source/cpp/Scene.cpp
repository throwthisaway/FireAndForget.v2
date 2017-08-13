#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"

void Scene::Object::Update(double frame, double total) {
	m = glm::translate(RotationMatrix(glm::translate(glm::mat4{}, pivot), rot.x, rot.y, rot.z), pos);
}

void Scene::Init(RendererWrapper* renderer) {
	renderer_ = renderer;
	assets_.Init(renderer);
	camera_.Perspective(400, 300);
	camera_.pos = {0.f, 0.f, -10.f};
	input.camera_ = &camera_;
	objects_.emplace_back(assets_.staticModels[Assets::PLACEHOLDER1]);
	objects_.emplace_back(assets_.staticModels[Assets::PLACEHOLDER2]);
}
void Scene::Render(size_t encoderIndex) {
	for (const auto& o : objects_)
		renderer_->SubmitToEncoder(encoderIndex, Materials::ColPos, (uint8_t*)&o.uniforms, {{o.model.vertices, o.model.color}, 0, o.model.count});
}
void Scene::Update(double frame, double total) {
	//mvp_ *= input.mat_;
	camera_.Update();
	for (auto& o : objects_) {
		o.Update(frame, total);
		o.uniforms.mvp =  camera_.vp * o.m;
	}
}
