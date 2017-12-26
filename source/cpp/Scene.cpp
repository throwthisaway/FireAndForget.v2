#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"
#include "CreateShaderParams.h"

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
		objects_.back().shaderParams.id = ShaderStructures::Pos;
		auto heapHandle = renderer->GetStaticShaderResourceHeap(2 * RendererWrapper::frameCount_);
		objects_.back().shaderParams.heapHandle = heapHandle;
		//auto a = CreateShaderParams<ShaderStructures::PosVSParams>(renderer, heapHandle);
		objects_.back().shaderParams.mvpStartIndex = renderer->GetShaderResourceIndex(heapHandle, sizeof(ShaderStructures::cMVP), RendererWrapper::frameCount_);
		objects_.back().shaderParams.colorStartIndex = renderer->GetShaderResourceIndex(heapHandle, sizeof(ShaderStructures::cColor), RendererWrapper::frameCount_);
	}
//	{
//		objects_.emplace_back(assets_.staticModels[Assets::CHECKERBOARD]);
//		//objects_.back().m = glm::translate(glm::mat4{},
//		objects_.back().pos = glm::vec3(.5f, .5f, .5f);
//		objects_.back().color = glm::vec4(0.f, .8f, .8f, 1.f);
//		objects_.back().shaderParams.id = ShaderStructures::Tex;
//		auto heapHandle = renderer->GetStaticShaderResourceHeap(3 * RendererWrapper::frameCount_ + 1);
//		objects_.back().shaderParams.heapHandle = heapHandle;
//		objects_.back().shaderParams.mvpStartIndex = renderer->GetShaderResourceIndex(heapHandle, sizeof(ShaderStructures::cMVP), RendererWrapper::frameCount_);
//		objects_.back().shaderParams.colorStartIndex = renderer->GetShaderResourceIndex(heapHandle, sizeof(ShaderStructures::cColor), RendererWrapper::frameCount_);
//		bool first = true;
//		for (auto& layer : objects_.back().mesh.layers) {
//			for (auto& submesh : layer.submeshes) {
//				if (submesh.material.color_tex_index != std::numeric_limits<size_t>::max()) {
//					objects_.back().shaderParams.colorTexSRVIndex = renderer->GetShaderResourceIndex(heapHandle, submesh.material.color_tex_index);
//					first = false;
//					break;
//				}
//			}
//			if (!first) break;
//		}
//	}

	{
		objects_.emplace_back(assets_.staticModels[Assets::BEETHOVEN]);
		objects_.back().pos = glm::vec3(.5f, 1.f, .5f);
		objects_.back().color = glm::vec4(.9f, .4f, .8f, 1.f);
		objects_.back().shaderParams.id = ShaderStructures::Pos;
		auto heapHandle = renderer->GetStaticShaderResourceHeap(2 * RendererWrapper::frameCount_);
		objects_.back().shaderParams.heapHandle = heapHandle;
		objects_.back().shaderParams.mvpStartIndex = renderer->GetShaderResourceIndex(heapHandle, sizeof(ShaderStructures::cMVP), RendererWrapper::frameCount_);
		objects_.back().shaderParams.colorStartIndex = renderer->GetShaderResourceIndex(heapHandle, sizeof(ShaderStructures::cColor), RendererWrapper::frameCount_);
	}

	// TODO:: remove
	m_radiansPerSecond = glm::quarter_pi<float>();
	m_angle = 0;
	m_tracking = false;
	// TODO:: remove 
}
void Scene::Render() {
	if (!assets_.loadCompleted) return;
	/*for (const auto& o : objects_)
		renderer_->SubmitToEncoder(encoderIndex, Materials::ColPos, (uint8_t*)&o.uniforms, o.model);*/
	//{
	//	const auto& o = objects_[1];
	//	renderer_->SubmitToEncoder(encoderIndex, Materials::ColPos, (uint8_t*)&os, o.model);
	//}
	for (const auto& o : objects_) {
		auto mvp = glm::transpose(camera_.vp * o.m);
		// TODO:: update resources according to selected encoderIndex/shaderid
		renderer_->UpdateShaderResource(o.shaderParams.mvpStartIndex + renderer_->GetCurrenFrameIndex(), &mvp, sizeof(ShaderStructures::cMVP));
		renderer_->UpdateShaderResource(o.shaderParams.colorStartIndex + renderer_->GetCurrenFrameIndex(), &o.color, sizeof(ShaderStructures::cColor));
		renderer_->SubmitToEncoder(o.shaderParams.id, o.shaderParams.id, o.shaderParams.heapHandle, { o.shaderParams.mvpStartIndex}, {o.shaderParams.colorStartIndex}, o.mesh);
		break;
	}
}
void Scene::Update(double frame, double total) {
	// TODO:: remove
	m_angle += static_cast<float>(total) * m_radiansPerSecond / 1000.f;
	// TODO:: remove

	camera_.Update();
	for (auto& o : objects_) {
		o.Update(frame, total);
	}
}
