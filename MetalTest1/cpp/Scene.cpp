#include "Scene.hpp"

void Scene::Init(RendererWrapper* renderer) {
	renderer_ = renderer;
	assets_.Init(renderer);
	camera_.Perspective(400, 300);
	camera_.pos = {0.f, 0.f, -10.f};
	input.camera_ = &camera_;
}
void Scene::Render() {
	renderer_->SubmitToPipeline(Materials::ColPos, (uint8_t*)&uniforms, {{assets_.placeholder1_.vertices, assets_.placeholder1_.color}, 0, assets_.placeholder1_.count});
	renderer_->SubmitToPipeline(Materials::ColPos, (uint8_t*)&uniforms, {{assets_.placeholder2_.vertices, assets_.placeholder2_.color}, 0, assets_.placeholder2_.count});
}
void Scene::Update(double frame, double total) {
	//mvp_ *= input.mat_;
	camera_.Update();
	uniforms.mvp =  camera_.vp * glm::mat4{};
}
