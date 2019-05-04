#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"
#include "CreateShaderParams.h"

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
void Scene::OnAssetsLoaded() {
	objects_.push_back({ lights_[0].pointLight.pos, {}, assets::Assets::LIGHT });
	lights_[0].placeholder = objects_.size() - 1;
	objects_.push_back({ lights_[1].pointLight.pos, {}, assets::Assets::LIGHT });
	lights_[1].placeholder = objects_.size() - 1;
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
			assets_.materialMap.emplace(L"mat_" + std::to_wstring(i) + L'_' + std::to_wstring(j), submesh.material);
		}
		pos.y += incY;
		pos.x = -3 * incX;
	}
	state = State::AssetsLoaded;
	PrepareScene();
}
void Scene::Init(RendererWrapper* renderer, int width, int height) {
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

	shaderResources.cScene = renderer->CreateShaderResource(sizeof(ShaderStructures::cScene), ShaderStructures::cScene::frame_count);
	shaderResources.cAO = renderer->CreateShaderResource(sizeof(ShaderStructures::cAO), ShaderStructures::cAO::frame_count);
#ifdef PLATFORM_WIN
	deferredBuffers.descAllocEntryIndex = renderer->AllocDescriptors(DeferredBuffers::Params::desc_count);
	uint16_t offset = 0;
	// cScene
	auto rootParamIndex = DeferredBuffers::Params::index<cScene>::value;
	deferredBuffers_.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
	for (uint32_t frame = 0; frame < cScene::frame_count; ++frame) {
		renderer->CreateCBV(deferredBuffers_.descAllocEntryIndex, offset, frame, shaderResources.cScene + frame);
	}
	++offset;
	// TODO:: cAO
	assert(false && "TODO:: cAO");
#elif defined(PLATFORM_MAC_OS)
	shaderStructures.cAO.ao.bias = 0.f;
	shaderStructures.cAO.ao.rad = .04f;
	shaderStructures.cAO.ao.scale = 1.f;
	shaderStructures.cAO.ao.intensity = 1.f;
	deferredBuffers_.fsBuffers[DeferredBuffers::FSParams::index<cScene>::value] = {shaderResources.cScene, cScene::frame_count};
	deferredBuffers_.fsBuffers[DeferredBuffers::FSParams::index<cAO>::value] = {shaderResources.cAO, cAO::frame_count};
#endif

	state = State::AssetsLoading;
	assets_.Init(renderer);
#ifdef PLATFORM_WIN
	assets_.loadCompleteTask.then([this, renderer](Concurrency::task<void>& assetsWhenAllCompletion) {
		assetsWhenAllCompletion.then([this]() { OnAssetsLoaded();});
	});
#elif defined(PLATFORM_MAC_OS)
	OnAssetsLoaded();
#endif
}

void Scene::PrepareScene() {
	state = State::PrepareScene;
	{
		Img::ImgData img = assets::Assets::LoadImage(L"random.png", Img::PixelFormat::RGBA8);
		deferredBuffers_.random = renderer_->CreateTexture(img.data.get(), img.width, img.height, img.pf);
		shaderStructures.cAO.ao.random_size.x = img.width;
		shaderStructures.cAO.ao.random_size.y = img.height;
		renderer_->UpdateShaderResource(shaderResources.cAO, &shaderStructures.cAO, sizeof(shaderStructures.cAO));

	}
	Img::ImgData img = assets::Assets::LoadImage(L"Alexs_Apt_2k.hdr"/*L"Serpentine_Valley_3k.hdr"*/, Img::PixelFormat::RGBAF32);
	const auto& mesh = assets_.models[assets::Assets::UNITCUBE];
	auto& l = mesh.layers.front();
	TextureIndex envMap = renderer_->CreateTexture(img.data.get(), img.width, img.height, img.pf);
	const uint64_t cubeEnvMapDim = 512;
	cubeEnv_ = renderer_->GenCubeMap(envMap, mesh.vb, mesh.ib, l.submeshes.front(), cubeEnvMapDim, ShaderStructures::CubeEnvMap, true,  "CubeEnvMap");
	//cubeEnv_ = renderer_->GenTestCubeMap();
	const uint64_t irradianceDim = 32;
	deferredBuffers_.irradiance = renderer_->GenCubeMap(cubeEnv_, mesh.vb, mesh.ib, l.submeshes.front(), irradianceDim, ShaderStructures::Irradiance, false, "Irradiance");
	const uint64_t preFilterEnvDim = 128;
	deferredBuffers_.prefilteredEnvMap = renderer_->GenPrefilteredEnvCubeMap(cubeEnv_, mesh.vb, mesh.ib, l.submeshes.front(), preFilterEnvDim, ShaderStructures::PrefilterEnv, "PrefilterEnv");
	const uint64_t brdfLUTDim = 512;
	deferredBuffers_.BRDFLUT = renderer_->GenBRDFLUT(brdfLUTDim, ShaderStructures::BRDFLUT, "BRDFLUT");
	renderer_->SetDeferredBuffers(deferredBuffers_);
	state = State::Ready;
}

void Scene::Render() {
	if (state == State::AssetsLoaded) PrepareScene();
	if (state != State::Ready) return;
	// bg
	renderer_->StartForwardPass();
	if (cubeEnv_ != InvalidTexture) {
		const auto& mesh = assets_.models[assets::Assets::UNITCUBE];
		auto& l = mesh.layers.front();
		ShaderStructures::BgCmd cmd{ camera_.vp, l.submeshes.front(), mesh.vb, mesh.ib, ShaderStructures::Bg, /*deferredBuffers_.prefilteredEnvMap*/cubeEnv_};
		renderer_->Submit(cmd);
	}

	renderer_->StartDeferredPass();
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
				ShaderId shader = (material.texAlbedo != InvalidTexture && submesh.vertexType == assets::VertexType::PNT) ? ShaderStructures::Tex : ShaderStructures::Pos;
				ShaderStructures::DrawCmd cmd{ m, mvp, submesh, material, mesh.vb, mesh.ib, shader};
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
	if (state == State::AssetsLoaded) PrepareScene();
	if (state != State::Ready) return;
	for (const auto& o : objects_) {
		assert(assets_.models[o.mesh].layers.front().submeshes.size() <= 12);
	}
	camera_.Update();
	// need to determine it from view because of ScreenSpaceRotator...
	shaderStructures.cScene.scene.eyePos = camera_.GetEyePos();
	// TODO:: WTF?
	shaderStructures.cScene.scene.ip = glm::inverse(camera_.proj);
	shaderStructures.cScene.scene.ivp = camera_.ivp;
	shaderStructures.cScene.scene.nf.x = camera_.n; shaderStructures.cScene.scene.nf.y = camera_.f;
	shaderStructures.cScene.scene.viewport.x = viewport_.width; shaderStructures.cScene.scene.viewport.y = viewport_.height;
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
