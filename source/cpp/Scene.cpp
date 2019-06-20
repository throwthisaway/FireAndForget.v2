#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"

void Scene::Object::Update(double frame, double total) {
	// TODO:: currently everything is in Scene::Update
}

namespace {
	const float defaultLightRange = 25.f;
	const ShaderStructures::PointLight defaultPointLight = { {300.f, 300.f, 300.f}, /*{.4f, .4f, .4f}*//*{23.47f, 21.31f, 20.79f}*//* diffuse */
		{.0f, .0f, .0f}, /* ambient */
		{.8f, .8f, .8f},/* specular highlight */
		{ 4.f, 4.f, 10.f}, /* position */
		{ 1.f, 2.f / defaultLightRange, 1.f / (defaultLightRange * defaultLightRange), defaultLightRange }, /* attenuation and range */};
}
void Scene::PrepareScene() {
	objects_.push_back({ lights_[0].pointLight.pos, {}, assets::Assets::LIGHT });
	lights_[0].placeholder = index_t(objects_.size() - 1);
	objects_.push_back({ lights_[1].pointLight.pos, {}, assets::Assets::LIGHT });
	lights_[1].placeholder = index_t(objects_.size() - 1);
	objects_.push_back({ {}, {}, assets::Assets::PLACEHOLDER });
	objects_.push_back({ {}, {}, assets::Assets::CHECKERBOARD });
	objects_.push_back({ { 0.f, .5f, 0.f }, {}, assets::Assets::BEETHOVEN });
	//objects_.push_back({ { 0.f, .0f, .0f }, {}, assets::Assets::UNITCUBE });
	const float incX = 2.4f, incY = 2.9f;
	auto pos = glm::vec3{ -3 * incX, -3 * incY, -2.f };
	const int count = 7;
	for (int i = 0; i < count; ++i) {
		for (int j = 0; j < count; ++j) {
			auto model = assets_.models[assets::Assets::SPHERE];
			assets_.models.push_back(model);
			objects_.push_back({ pos, {}, index_t(assets_.models.size() - 1) });
			assets_.materials.push_back({ { .5f, .5f, .5f },
				glm::clamp((float)j / count, .05f, 1.f),
				(float)i / count,
				InvalidTexture });
			pos.x += incX;
			auto& submesh = assets_.models[objects_.back().mesh].layers.front().submeshes.front();
			submesh.material = (MaterialIndex)assets_.materials.size() - 1;
		}
		pos.y += incY;
		pos.x = -3 * incX;
	}

	deferredCmd_.ao.bias = 0.f;
	deferredCmd_.ao.rad = .04f;
	deferredCmd_.ao.scale = 1.f;
	deferredCmd_.ao.intensity = 1.f;
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		deferredCmd_.scene.light[i] = lights_[i].pointLight;
		//FromVec3(camera_.view * glm::vec4(ToVec3(lights_[i].pointLight.pos), 1.f), shaderStructures.cScene.scene.light[i].pos);
	}
	renderer_->BeginPrePass();
	deferredCmd_.random = assets_.textures[assets::Assets::RANDOM];
	Dim dim = renderer_->GetDimensions(deferredCmd_.random);
	deferredCmd_.ao.random_size.x = (float)dim.w;
	deferredCmd_.ao.random_size.y = (float)dim.h;
	const auto& mesh = assets_.models[assets::Assets::UNITCUBE];
	auto& l = mesh.layers.front();
	TextureIndex envMap = assets_.textures[assets::Assets::ENVIRONMENT_MAP];
	const uint64_t cubeEnvMapDim = 512;
	cubeEnv_ = renderer_->GenCubeMap(envMap, mesh.vb, mesh.ib, l.submeshes.front(), cubeEnvMapDim, ShaderStructures::CubeEnvMap, true,  L"CubeEnvMap");
	//cubeEnv_ = renderer_->GenTestCubeMap();
	const uint64_t irradianceDim = 32;
	deferredCmd_.irradiance = renderer_->GenCubeMap(cubeEnv_, mesh.vb, mesh.ib, l.submeshes.front(), irradianceDim, ShaderStructures::Irradiance, false, L"Irradiance");
	const uint64_t preFilterEnvDim = 128;
	deferredCmd_.prefilteredEnvMap = renderer_->GenPrefilteredEnvCubeMap(cubeEnv_, mesh.vb, mesh.ib, l.submeshes.front(), preFilterEnvDim, ShaderStructures::PrefilterEnv, L"PrefilterEnv");
	const uint64_t brdfLUTDim = 512;
	deferredCmd_.BRDFLUT = renderer_->GenBRDFLUT(brdfLUTDim, ShaderStructures::BRDFLUT, L"BRDFLUT");
	renderer_->EndPrePass();
	state = State::Ready;
}
void Scene::Init(Renderer* renderer, int width, int height) {
	state = State::Start;
	using namespace ShaderStructures;
	renderer_ = renderer;
	viewport_.width = width; viewport_.height = height;
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
#ifdef PLATFORM_WIN

#elif defined(PLATFORM_MAC_OS)
	deferredBuffers_.fsBuffers[DeferredBuffers::FSParams::index<cScene>::value] = {shaderResources.cScene, cScene::frame_count};
	deferredBuffers_.fsBuffers[DeferredBuffers::FSParams::index<cAO>::value] = {shaderResources.cAO, cAO::frame_count};
#endif

	state = State::AssetsLoading;
	assets_.Init();
#ifdef PLATFORM_WIN
#elif defined(PLATFORM_MAC_OS)
	OnAssetsLoaded();
#endif
}

bool Scene::Render() {
	if (state != State::Ready && assets_.status == assets::Assets::Status::kReady) {
		PrepareScene();
		state = State::Ready;
	}
	if (state != State::Ready) return false;
	renderer_->BeginRender();
	// bg
	renderer_->StartForwardPass();
	if (cubeEnv_ != InvalidTexture) {
		const auto& mesh = assets_.models[assets::Assets::UNITCUBE];
		auto& l = mesh.layers.front();
		ShaderStructures::BgCmd cmd{ camera_.vp, l.submeshes.front(), mesh.vb, mesh.ib, ShaderStructures::Bg, /*deferredBuffers_.prefilteredEnvMap*/cubeEnv_};
		renderer_->Submit(cmd);
	}

	renderer_->StartGeometryPass();
	for (const auto& o : objects_) {
		assert(assets_.models[o.mesh].layers.front().submeshes.size() <= 12);
		const auto& mesh = assets_.models[o.mesh];
		for (const auto& l : mesh.layers) {
			auto m = this->m * glm::translate(RotationMatrix(o.rot.x, o.rot.y, o.rot.z), o.pos);
			m[3] += float4(l.pivot, 0.f);
			auto mvp = glm::transpose(camera_.vp * m);
			m = glm::transpose(m);
			for (const auto& submesh : l.submeshes) {
				const auto& material = assets_.materials[submesh.material];
				ShaderId shader = (material.texAlbedo != InvalidTexture && submesh.vertexType == VertexType::PNT) ? ShaderStructures::Tex : ShaderStructures::Pos;
				ShaderStructures::DrawCmd cmd{ m, mvp, submesh, material, mesh.vb, mesh.ib, shader};
				renderer_->Submit(cmd);
			}
		}
	}
	
	renderer_->DoLightingPass(deferredCmd_);
	return renderer_->Render();
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
	assets_.Update(renderer_);
	if (state != State::Ready) return;
	for (const auto& o : objects_) {
		assert(assets_.models[o.mesh].layers.front().submeshes.size() <= 12);
	}
	camera_.Update();
	// need to determine it from view because of ScreenSpaceRotator...
	deferredCmd_.scene.eyePos = camera_.GetEyePos();
	// TODO:: WTF?
	deferredCmd_.scene.ip = glm::inverse(camera_.proj);
	deferredCmd_.scene.ivp = camera_.ivp;
	deferredCmd_.scene.nf.x = camera_.n; deferredCmd_.scene.nf.y = camera_.f;
	deferredCmd_.scene.viewport.x = viewport_.width; deferredCmd_.scene.viewport.y = viewport_.height;
	for (auto& o : objects_) {
		o.Update(frame, total);
	}
	// TODO:: update light pos here if neccessay
	objects_[lights_[0].placeholder].pos = lights_[0].pointLight.pos;
	objects_[lights_[1].placeholder].pos = lights_[1].pointLight.pos;
}
