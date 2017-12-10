#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"
#include "../Content/DescriptorHeapAllocator.h"
#include "../Common/DeviceResources.h"

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
	/*objects_.emplace_back(assets_.staticModels[Assets::PLACEHOLDER1]);
	objects_.emplace_back(assets_.staticModels[Assets::PLACEHOLDER2]);*/
	{
		objects_.emplace_back(assets_.staticModels[Assets::CHECKERBOARD]);
		objects_.back().color = glm::vec4(0.5f, .8f, .0f, 1.f);
		//objects_.back().m = {};
		objects_.back().material.id = GPUMaterials::Pos;
		objects_.back().material.mvpStartIndex = renderer->GetShaderResources().staticResources_.Push<GPUMaterials::cMVP>(DX::c_frameCount);
		objects_.back().material.colorStartIndex = renderer->GetShaderResources().staticResources_.Push<GPUMaterials::cColor>(DX::c_frameCount);
	}
	{
		objects_.emplace_back(assets_.staticModels[Assets::CHECKERBOARD]);
		//objects_.back().m = glm::translate(glm::mat4{}, 
		objects_.back().pos = glm::vec3(.5f, .5f, .5f);
		objects_.back().color = glm::vec4(0.f, .8f, .8f, 1.f);
		objects_.back().material.id = GPUMaterials::Pos;
		objects_.back().material.mvpStartIndex = renderer->GetShaderResources().staticResources_.Push<GPUMaterials::cMVP>(DX::c_frameCount);
		objects_.back().material.colorStartIndex = renderer->GetShaderResources().staticResources_.Push<GPUMaterials::cColor>(DX::c_frameCount);
	}

	{
		objects_.emplace_back(assets_.staticModels[Assets::BEETHOVEN]);
		objects_.back().pos = glm::vec3(.5f, 1.f, .5f);
		objects_.back().color = glm::vec4(.9f, .4f, .8f, 1.f);
		objects_.back().material.id = GPUMaterials::Pos;
		objects_.back().material.mvpStartIndex = renderer->GetShaderResources().staticResources_.Push<GPUMaterials::cMVP>(DX::c_frameCount);
		objects_.back().material.colorStartIndex = renderer->GetShaderResources().staticResources_.Push<GPUMaterials::cColor>(DX::c_frameCount);
	}

	// TODO:: remove
	m_radiansPerSecond = glm::quarter_pi<float>();
	m_angle = 0;
	m_tracking = false;
	// TODO:: remove 
}
void Scene::Render( size_t encoderIndex) {
	if (!assets_.loadCompleted) return;
	/*for (const auto& o : objects_)
		renderer_->SubmitToEncoder(encoderIndex, Materials::ColPos, (uint8_t*)&o.uniforms, o.model);*/
	//{
	//	const auto& o = objects_[1];
	//	renderer_->SubmitToEncoder(encoderIndex, Materials::ColPos, (uint8_t*)&os, o.model);
	//}
	for (const auto& o : objects_) {
		// Update resources
		auto& mvpResource = renderer_->GetShaderResources().staticResources_.Get(o.material.mvpStartIndex + renderer_->GetCurrenFrameIndex());
		auto mvp = glm::transpose(camera_.vp * o.m);
		memcpy(mvpResource.destination, &mvp, sizeof(GPUMaterials::cMVP));
		auto& colorResource = renderer_->GetShaderResources().staticResources_.Get(o.material.colorStartIndex + renderer_->GetCurrenFrameIndex());
		memcpy(colorResource.destination, &o.color, sizeof(GPUMaterials::cColor));

		renderer_->SubmitToEncoder(encoderIndex, GPUMaterials::Pos, { o.material.mvpStartIndex, o.material.colorStartIndex}, o.mesh);
	}
}
void Scene::Update(double frame, double total) {
	// TODO:: remove
	m_angle += static_cast<float>(total) * m_radiansPerSecond / 1000.f;
	// TODO:: remove

	camera_.Update();
	for (auto& o : objects_) {
		o.Update(frame, total);
#ifdef PLATFORM_WIN

		/*memcpy(&o.uniforms.projection, &glm::transpose(camera_.proj), sizeof(o.uniforms.projection));
		memcpy(&o.uniforms.view, &glm::transpose(camera_.view), sizeof(o.uniforms.view));
		DirectX::XMStoreFloat4x4(&o.uniforms.model, DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationY(m_angle)));*/
		// TODO::
#else
		o.uniforms.mvp =  camera_.vp * o.m;
#endif
	}
}
