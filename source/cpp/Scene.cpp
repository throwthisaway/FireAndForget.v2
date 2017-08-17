#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"

void Scene::Object::Update(double frame, double total) {
	m = glm::translate(RotationMatrix(glm::translate(glm::mat4{}, pivot), rot.x, rot.y, rot.z), pos);
}

void Scene::Init(RendererWrapper* renderer, int width, int height) {
	renderer_ = renderer;
	camera_.Perspective(width, height);
#ifdef PLATFORM_WIN
	camera_.pos = { 0.f, 0.f, -1.5f };
#else
	camera_.pos = {0.f, 0.f, -10.f};
#endif
	input.camera_ = &camera_;

	assets_.Init(renderer);
	objects_.emplace_back(assets_.staticModels[Assets::PLACEHOLDER1]);
	objects_.emplace_back(assets_.staticModels[Assets::PLACEHOLDER2]);

	// TODO:: remove
	m_radiansPerSecond = glm::quarter_pi<float>();
	m_angle = 0;
	m_tracking = false;
	// TODO:: remove 
}
void Scene::Render(size_t encoderIndex) {
	if (!assets_.loadCompleted) return;
	for (const auto& o : objects_)
		renderer_->SubmitToEncoder(encoderIndex, Materials::ColPos, (uint8_t*)&o.uniforms, o.model);
}
void Scene::Update(double frame, double total) {
	// TODO:: remove
	m_angle += static_cast<float>(total) * m_radiansPerSecond / 1000.f;
	// TODO:: remove

	camera_.Update();
	for (auto& o : objects_) {
		o.Update(frame, total);
#ifdef PLATFORM_WIN
		memcpy(&o.uniforms.projection, &glm::transpose(camera_.proj), sizeof(o.uniforms.projection));
		memcpy(&o.uniforms.view, &glm::transpose(camera_.view), sizeof(o.uniforms.view));
		DirectX::XMStoreFloat4x4(&o.uniforms.model, DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationY(m_angle)));
#else
		o.uniforms.mvp =  camera_.vp * o.m;
#endif
	}
}
