#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"
#include "CreateShaderParams.h"

namespace{
inline vec3_t ToVec3(const float* v) {
	return { v[0], v[1], v[2] };
}

inline void FromVec3(const vec3_t& v, float* out) {
	out[0] = v.x; out[1] = v.y; out[2] = v.z;
}
}

void Scene::Object::Update(double frame, double total) {
	// TODO:: currently everything is in Scene::Update
}

namespace {
	const float defaultLightRange = 25.f;
	const ShaderStructures::PointLight defaultPointLight = { {300.f, 300.f, 300.f}, /*{.4f, .4f, .4f}*//*{23.47f, 21.31f, 20.79f}*//* diffuse */
		{.0f, .0f, .0f}, /* ambient */
		{.8f, .8f, .8f},/* specular highlight */
		{ 4.f, 4.f, 10.f}, /* position */
		{ 1.f, 2.f / defaultLightRange, 1.f / (defaultLightRange * defaultLightRange) }, /* attenuation */
		defaultLightRange /* range */ };
}
void Scene::OnAssetsLoaded() {
	objects_.push_back({ lights_[0].pointLight.pos, {}, assets::Assets::LIGHT });
	lights_[0].placeholder = objects_.size() - 1;
	objects_.push_back({ lights_[1].pointLight.pos, {}, assets::Assets::LIGHT });
	lights_[1].placeholder = objects_.size() - 1;
	objects_.push_back({ {}, {}, assets::Assets::PLACEHOLDER });
	objects_.push_back({ {}, {}, assets::Assets::CHECKERBOARD });
	objects_.push_back({ { 0.f, .5f, 0.f }, {}, assets::Assets::BEETHOVEN });
	const float incX = 2.4f, incY = 2.9f;
	auto pos = glm::vec3{ -3 * incX, -3 * incY, -2.f };
	for (int i = 0; i < 7; ++i) {
		for (int j = 0; j < 7; ++j) {
			auto model = assets_.models[assets::Assets::SPHERE];
			assets_.models.push_back(model);
			objects_.push_back({ pos, {}, index_t(assets_.models.size() - 1) });
			assets_.materials.push_back({ { .8f, .0f, .0f },
				glm::clamp(j / 6.f, 0.025f, 1.f),
				i / 6.f,
				InvalidTexture });
			pos.x += incX;
			auto& submesh = assets_.models[objects_.back().mesh].layers.front().submeshes.front();
			submesh.material = (MaterialIndex)assets_.materials.size() - 1;
			assets_.materialMap.emplace(L"mat_" + std::to_wstring(i) + L'_' + std::to_wstring(j), submesh.material);
		}
		pos.y += incY;
		pos.x = -3 * incX;
	}
	loadCompleted = true;
}
void Scene::Init(RendererWrapper* renderer, int width, int height) {
	using namespace ShaderStructures;
	renderer_ = renderer;
	camera_.Perspective(width, height);
	
	const float Z = -10.5f;
	camera_.Translate({ 0.f, 0.f, Z });
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		lights_[i].pointLight = defaultPointLight;
	}
	lights_[0].pointLight.pos[0] = lights_[0].pointLight.pos[1]= -4.f;
//	shaderStructures.cScene.scene.light[1].diffuse[0] = 150.0f;
//	shaderStructures.cScene.scene.light[1].diffuse[1] = 150.0f;
//	shaderStructures.cScene.scene.light[1].diffuse[2] = 150.0f;

	shaderResources.cScene = renderer->CreateShaderResource(sizeof(ShaderStructures::cScene), ShaderStructures::cScene::frame_count);
	DeferredBuffers deferredBuffers;
#ifdef PLATFORM_WIN
	deferredBuffers.descAllocEntryIndex = renderer->AllocDescriptors(DeferredBuffers::Params::desc_count);
	uint16_t offset = 0;
	// cScene
	auto rootParamIndex = DeferredBuffers::Params::index<cScene>::value;
	deferredBuffers.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
	for (uint32_t frame = 0; frame < cScene::frame_count; ++frame) {
		renderer->CreateCBV(deferredBuffers.descAllocEntryIndex, offset, frame, shaderResources.cScene + frame);
	}
	++offset;
#elif defined(PLATFORM_MAC_OS)
	deferredBuffers.fsBuffers[DeferredBuffers::FSParams::index<cScene>::value] = {shaderResources.cScene, cScene::frame_count};
#endif
	renderer->SetDeferredBuffers(deferredBuffers);

	assets_.Init(renderer);
#ifdef PLATFORM_WIN
	assets_.loadCompleteTask.then([this, renderer](Concurrency::task<void>& assetsWhenAllCompletion) {
		assetsWhenAllCompletion.then([this]() { OnAssetsLoaded();});
	});
#elif defined(PLATFORM_MAC_OS)
	OnAssetsLoaded();
#endif
}
void Scene::Render() {
	if (!loadCompleted) return;
	for (const auto& o : objects_) {
		assert(assets_.models[o.mesh].layers.front().submeshes.size() <= 12);
		const auto& mesh = assets_.models[o.mesh];
		for (const auto& l : mesh.layers) {
			auto m = this->m * glm::translate(RotationMatrix(o.rot.x, o.rot.y, o.rot.z), o.pos);
			m[3] += float4(l.pivot, 0.f);
			auto mvp = glm::transpose(camera_.vp * m);
			for (const auto& submesh : l.submeshes) {
				ShaderStructures::DrawCmd cmd{ m, mvp, mesh.vb, mesh.ib, submesh, assets_.materials[submesh.material] };
				renderer_->Submit(cmd);
			}
		}
	}
}
void Scene::UpdateCameraTransform() {
	camera_.Translate(input.dpos);
	camera_.RotatePreMultiply(input.drot);
}

void Scene::UpdateSceneTransform() {
	transform.pos += input.dpos;
	auto rot = glm::inverse(glm::mat3(camera_.view)) * input.drot;
	// wrong: transform.rot += rot;
	// TODO:: transform is not well maintained...
	m = ScreenSpaceRotator(m, Transform{ transform.pos, transform.center, rot });
}
void Scene::Update(double frame, double total) {
	if (!loadCompleted) return;
	for (const auto& o : objects_) {
		assert(assets_.models[o.mesh].layers.front().submeshes.size() <= 12);
	}
	camera_.Update();
	// need to determine it from view because of ScreenSpaceRotator...
	shaderStructures.cScene.scene.eyePos = camera_.GetEyePos();
	// TODO:: WTF?
	shaderStructures.cScene.scene.ip = glm::inverse(camera_.proj);
	shaderStructures.cScene.scene.ivp = camera_.ivp;
	shaderStructures.cScene.scene.n = camera_.n; shaderStructures.cScene.scene.f = camera_.f;
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		shaderStructures.cScene.scene.light[i] = lights_[i].pointLight;
		//FromVec3(camera_.view * glm::vec4(ToVec3(lights_[i].pointLight.pos), 1.f), shaderStructures.cScene.scene.light[i].pos);
	}
	renderer_->UpdateShaderResource(shaderResources.cScene + renderer_->GetCurrenFrameIndex(), &shaderStructures.cScene, sizeof(shaderStructures.cScene));
	for (auto& o : objects_) {
		o.Update(frame, total);
	}
	// TODO:: update light pos here if neccessay
	objects_[lights_[0].placeholder].pos = lights_[0].pointLight.pos;
	objects_[lights_[1].placeholder].pos = lights_[1].pointLight.pos;
}
