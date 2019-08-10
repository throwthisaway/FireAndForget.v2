#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"

void Scene::Object::Update(double frame, double total) {
	// TODO:: currently everything is in Scene::Update
}

namespace {
	const float defaultLightRange = 25.f;
	const PointLight defaultPointLight = { {300.f, 300.f, 300.f}, /*{.4f, .4f, .4f}*//*{23.47f, 21.31f, 20.79f}*//* diffuse */
		{.0f, .0f, .0f}, /* ambient */
		{.8f, .8f, .8f},/* specular highlight */
		{ 4.f, 4.f, -10.f}, /* position */
		{ 1.f, 2.f / defaultLightRange, 1.f / (defaultLightRange * defaultLightRange), defaultLightRange }, /* attenuation and range */};
	ShaderId SelectModoShader(uint32_t uvCount, uint32_t textureMask) {
		switch(uvCount) {
			case 0:
				assert(!textureMask);
				return ShaderStructures::Pos;
			case 1: {
				switch(textureMask) {
					case 0xf:
						return ShaderStructures::ModoDNMR;
					case 0x3:
						return ShaderStructures::ModoDN;
					default: assert(false);
				}
			}
		}
		assert(false);
		return (uint16_t)-1;
	}
}
void Scene::PrepareScene() {
	if (prepared_) return;
	prepared_ = true;
	objects_.push_back({ lights_[0].pointLight.pos, {}, assets::Assets::LIGHT });
	lights_[0].placeholder = index_t(objects_.size() - 1);
	objects_.push_back({ lights_[1].pointLight.pos, {}, assets::Assets::LIGHT });
	lights_[1].placeholder = index_t(objects_.size() - 1);
	objects_.push_back({ {}, {}, assets::Assets::PLACEHOLDER });
	objects_.push_back({ {}, {}, assets::Assets::CHECKERBOARD });
	objects_.push_back({ { 0.f, .5f, 0.f }, {}, assets::Assets::BEETHOVEN });
	for (int i = 0; i < assets_.meshes.size(); ++i) {
//		auto& mesh = assets_.meshes[i];
		modoObjects_.push_back({{ 0.f, -.5f * i, -5.f * (i+1)}, {}, (index_t)i});
	}
	//objects_.push_back({ { 0.f, .0f, .0f }, {}, assets::Assets::UNITCUBE });
	const float incX = 2.4f, incY = 2.9f;
	auto pos = glm::vec3{ -3 * incX, -3 * incY, 2.f };
	const int count = 7;
	for (int i = 0; i < count; ++i) {
		for (int j = 0; j < count; ++j) {
			auto model = assets_.models[assets::Assets::SPHERE];
			assets_.models.push_back(model);
			objects_.push_back({ pos, {}, index_t(assets_.models.size() - 1) });
			assets_.materials.push_back({ { .5f, .0f, .0f },
				{glm::clamp((float)j / count, .05f, 1.f), (float)i / count } });
			pos.x += incX;
			auto& submesh = assets_.models[objects_.back().mesh].layers.front().submeshes.front();
			submesh.material = (MaterialIndex)assets_.materials.size() - 1;
			submesh.texAlbedo = InvalidTexture;
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
	cubeEnv_ = renderer_->GenCubeMap(envMap, mesh.vb, mesh.ib, l.submeshes.front(), cubeEnvMapDim, ShaderStructures::CubeEnvMap, true,  "CubeEnvMap");
	//cubeEnv_ = renderer_->GenTestCubeMap();
	const uint64_t irradianceDim = 32;
	deferredCmd_.irradiance = renderer_->GenCubeMap(cubeEnv_, mesh.vb, mesh.ib, l.submeshes.front(), irradianceDim, ShaderStructures::Irradiance, false, "Irradiance");
	const uint64_t preFilterEnvDim = 128;
	deferredCmd_.prefilteredEnvMap = renderer_->GenPrefilteredEnvCubeMap(cubeEnv_, mesh.vb, mesh.ib, l.submeshes.front(), preFilterEnvDim, ShaderStructures::PrefilterEnv, "PrefilterEnv");
	const uint64_t brdfLUTDim = 512;
	deferredCmd_.BRDFLUT = renderer_->GenBRDFLUT(brdfLUTDim, ShaderStructures::BRDFLUT, "BRDFLUT");
	renderer_->EndPrePass();
	state = State::Ready;
}
void Scene::Init(Renderer* renderer, int width, int height) {
	state = State::Start;
	m = glm::identity<glm::mat4x4>();
	using namespace ShaderStructures;
	renderer_ = renderer;
	viewport_.width = width; viewport_.height = height;
	camera_.Perspective(width, height);

	for (int i = 0; i < MAX_LIGHTS; ++i) {
		lights_[i].pointLight = defaultPointLight;
	}
	lights_[0].pointLight.pos[0] = lights_[0].pointLight.pos[1]= -4.f;
//	shaderStructures.cScene.scene.light[1].diffuse[0] = 150.0f;
//	shaderStructures.cScene.scene.light[1].diffuse[1] = 150.0f;
//	shaderStructures.cScene.scene.light[1].diffuse[2] = 150.0f;
	state = State::AssetsLoading;
	assets_.Init(renderer);
}

void Scene::Render() {
	if (state != State::Ready && assets_.status == assets::Assets::Status::kReady) {
		state = State::Ready;
	}
	if (state != State::Ready) return;
	renderer_->BeginRender();
	PrepareScene();
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
			auto mvp = camera_.vp * m;
			for (const auto& submesh : l.submeshes) {
				const auto& material = assets_.materials[submesh.material];
				ShaderId shader = (submesh.texAlbedo != InvalidTexture && submesh.vertexType == VertexType::PNT) ? ShaderStructures::Tex : ShaderStructures::Pos;
				ShaderStructures::DrawCmd cmd{ m, mvp, submesh, material, mesh.vb, mesh.ib, shader};
				renderer_->Submit(cmd);
			}
		}
	}

	for (const auto& o : modoObjects_) {
		const auto& mesh = assets_.meshes[o.mesh];
		auto m = this->m * glm::translate(RotationMatrix(o.rot.x, o.rot.y, o.rot.z), o.pos);
		//m[3] += float4(l.pivot, 0.f);
		auto mvp = camera_.vp * m;
		for (const auto& submesh : mesh.submeshes) {
			ShaderId shader = SelectModoShader(submesh.uvCount, submesh.textureMask);
			ShaderStructures::ModoDrawCmd cmd{ {m, mvp}, submesh, submesh.material, mesh.vb, mesh.ib, shader };
			renderer_->Submit(cmd);
		}
	}
	renderer_->DoLightingPass(deferredCmd_);
	renderer_->Render();
}
void Scene::UpdateCameraTransform() {
	camera_.Translate(input.dpos);
	camera_.Rotate(input.drot);
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
	deferredCmd_.scene.eyePos = camera_.pos;
	// TODO:: WTF?
	deferredCmd_.scene.ip = glm::inverse(camera_.proj);
	deferredCmd_.scene.ivp = camera_.ivp;
	deferredCmd_.scene.nf.x = camera_.n; deferredCmd_.scene.nf.y = camera_.f;
	deferredCmd_.scene.viewport = { (float)viewport_.width, (float)viewport_.height };
	for (auto& o : objects_) {
		o.Update(frame, total);
	}
	// TODO:: update light pos here if neccessay
	objects_[lights_[0].placeholder].pos = lights_[0].pointLight.pos;
	objects_[lights_[1].placeholder].pos = lights_[1].pointLight.pos;
}
