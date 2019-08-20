#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"
#include "Content\UI.h"
#include "imgui.h"

void Scene::Object::Update(double frame, double total) {
	// TODO:: currently everything is in Scene::Update
}

namespace {
	constexpr float defaultLightRange = 25.f;
	float3 CalcAtt(float range) {
		return float3(1.f, 1.f / range, 1.f / (range * range));
	}
	const PointLight defaultPointLight = { {300.f, 300.f, 300.f}, /*{.4f, .4f, .4f}*//*{23.47f, 21.31f, 20.79f}*//* diffuse */
		{.0f, .0f, .0f}, /* ambient */
		{.8f, .8f, .8f},/* specular highlight */
		{ 4.f, 4.f, -10.f}, /* position */
		{CalcAtt(defaultLightRange), defaultLightRange }, /* attenuation and range */};
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
	objects_.push_back({ deferredCmd_.scene.light[0].pos, {}, assets::Assets::LIGHT });
	lights_[0].placeholder = index_t(objects_.size() - 1);
	objects_.push_back({ deferredCmd_.scene.light[1].pos, {}, assets::Assets::LIGHT });
	lights_[1].placeholder = index_t(objects_.size() - 1);
	objects_.push_back({ {}, {}, assets::Assets::PLACEHOLDER });
	objects_.push_back({ {}, {}, assets::Assets::CHECKERBOARD });
	objects_.push_back({ { 0.f, .5f, 0.f }, {}, assets::Assets::BEETHOVEN });
	float y = -.5f;
	for (int i = assets::Assets::STATIC_MODEL_COUNT; i < assets_.meshes.size(); ++i) {
//		auto& mesh = assets_.meshes[i];
		modoObjects_.push_back({{ 0.f, y, 0.f}, {}, (index_t)i});
		y += 1.f;
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
		deferredCmd_.scene.light[i] = defaultPointLight;
		//FromVec3(camera_.view * glm::vec4(ToVec3(lights_[i].pointLight.pos), 1.f), shaderStructures.cScene.scene.light[i].pos);
	}
	deferredCmd_.scene.light[0].pos.x = deferredCmd_.scene.light[0].pos.y = -4.f;
	renderer_->BeginPrePass();
	deferredCmd_.random = assets_.textures[assets::Assets::RANDOM];
	Dim dim = renderer_->GetDimensions(deferredCmd_.random);
	deferredCmd_.ao.random_size.x = (float)dim.w;
	deferredCmd_.ao.random_size.y = (float)dim.h;
	const auto& mesh = assets_.meshes[assets::Assets::UNITCUBE];
	TextureIndex envMap = assets_.textures[assets::Assets::ENVIRONMENT_MAP];
	const uint64_t cubeEnvMapDim = 512;
	cubeEnv_ = renderer_->GenCubeMap(envMap, mesh.vb, mesh.ib, mesh.submeshes.front(), cubeEnvMapDim, ShaderStructures::CubeEnvMap, true,  "CubeEnvMap");
	//cubeEnv_ = renderer_->GenTestCubeMap();
	const uint64_t irradianceDim = 32;
	deferredCmd_.irradiance = renderer_->GenCubeMap(cubeEnv_, mesh.vb, mesh.ib, mesh.submeshes.front(), irradianceDim, ShaderStructures::Irradiance, false, "Irradiance");
	const uint64_t preFilterEnvDim = 128;
	deferredCmd_.prefilteredEnvMap = renderer_->GenPrefilteredEnvCubeMap(cubeEnv_, mesh.vb, mesh.ib, mesh.submeshes.front(), preFilterEnvDim, ShaderStructures::PrefilterEnv, "PrefilterEnv");
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

//	shaderStructures.cScene.scene.light[1].diffuse[0] = 150.0f;
//	shaderStructures.cScene.scene.light[1].diffuse[1] = 150.0f;
//	shaderStructures.cScene.scene.light[1].diffuse[2] = 150.0f;
	state = State::AssetsLoading;
	assets_.Init(renderer);
}

void Scene::Render() {
	if (state != State::Ready) return;
	renderer_->BeginRender();
	PrepareScene();
	// bg
	renderer_->StartForwardPass();
	if (cubeEnv_ != InvalidTexture) {
		const auto& mesh = assets_.meshes[assets::Assets::UNITCUBE];
		ShaderStructures::BgCmd cmd{ camera_.vp, mesh.submeshes.front(), mesh.vb, mesh.ib, ShaderStructures::Bg, /*deferredCmd_.irradiance*/cubeEnv_};
		renderer_->Submit(cmd);
	}

	renderer_->StartGeometryPass();
	/*for (const auto& o : objects_) {
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
	}*/

	for (const auto& o : modoObjects_) {
		const auto& mesh = assets_.meshes[o.mesh];
		auto m = glm::translate(RotationMatrix(o.rot.x, o.rot.y, o.rot.z), o.pos);
		//m[3] += float4(l.pivot, 0.f);
		auto mvp = camera_.vp * m;
		for (const auto& submesh : mesh.submeshes) {
			ShaderId shader = SelectModoShader(submesh.uvCount, submesh.textureMask);
			ShaderStructures::ModoDrawCmd cmd{ {mvp, m}, submesh, submesh.material, mesh.vb, mesh.ib, shader };
			renderer_->Submit(cmd);
		}
	}
	renderer_->DoLightingPass(deferredCmd_);
	renderer_->Render();
}
void Scene::UpdateCameraTransform() {
	
	camera_.Rotate(input.drot);
	camera_.Translate(input.dpos);
}
void Scene::ObjectsWindow() {
	ImGui::SetNextWindowPos(ImVec2(20, 300), ImGuiCond_FirstUseEver);
	ImGui::Begin("Objects Window");
	int id = 0;
	for (auto& o : modoObjects_) {
		ImGui::Text("%f %f %f | %f %f %f", o.pos.x, o.pos.y, o.pos.z, o.rot.x, o.rot.y, o.rot.z);
		auto idMetallic = std::string("Metallic###Metallic") + std::to_string(++id);
		ImGui::SliderFloat(idMetallic.c_str(), &assets_.meshes[o.mesh].submeshes[0].material.metallic_roughness.x, 0.f, 1.f);
		auto idRoughness = std::string("Roughness###Roughness1") + std::to_string(++id);
		ImGui::SliderFloat(idRoughness.c_str(), &assets_.meshes[o.mesh].submeshes[0].material.metallic_roughness.y, 0.f, 1.f);
		ImGui::Separator();
	}
	ImGui::End();
}
void Scene::SceneWindow() {
	ImGui::SetNextWindowPos(ImVec2(20, 550), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(500, 650), ImGuiCond_FirstUseEver);
	ImGui::Begin("Scene Window");
	if (ImGui::CollapsingHeader("Camera", &ui.cameraOpen)) {
		ImGui::DragFloat3("Pos", (float*)& camera_.pos, 0.f, 100.f);
		ImGui::DragFloat3("Rot", (float*)& camera_.rot, -glm::pi<float>(), glm::pi<float>());
		ImGui::DragFloat3("EyePos", (float*)& camera_.eyePos, 0.f, 100.f);
	}
	if (ImGui::CollapsingHeader("AO", &ui.aoOpen)) {
		ImGui::SliderFloat("Intensity", &deferredCmd_.ao.intensity, 0.f, 1.f);
		ImGui::SliderFloat("Radius", &deferredCmd_.ao.rad, 0.f, .1f);
		ImGui::SliderFloat("Scale", &deferredCmd_.ao.scale, 0.f, 2.f);
		ImGui::SliderFloat("Bias", &deferredCmd_.ao.bias, 0.f, 1.f);
	}
	if (ImGui::CollapsingHeader("Lights", &ui.lightOpen)) {
		for (int i = 0; i < MAX_LIGHTS; ++i) {
			std::string id("Diffuse###Diffuse" + std::to_string(i));
			ImGui::ColorEdit3(id.c_str(), (float*)&deferredCmd_.scene.light[i].diffuse);
			id = "Specular###Specular" + std::to_string(i);
			ImGui::ColorEdit3(id.c_str(), (float*)&deferredCmd_.scene.light[i].specular);
			id = "Ambient###Ambient" + std::to_string(i);
			ImGui::ColorEdit3(id.c_str(), (float*)&deferredCmd_.scene.light[i].ambient);
			id = "Range###Range" + std::to_string(i);
			ImGui::SliderFloat(id.c_str(), &deferredCmd_.scene.light[i].att_range.w, 1.f, 500.f);
			deferredCmd_.scene.light[i].att_range = float4(CalcAtt(deferredCmd_.scene.light[i].att_range.w), deferredCmd_.scene.light[i].att_range.w);
			ImGui::Text("Attenuation: %f %f %f", deferredCmd_.scene.light[i].att_range.x, deferredCmd_.scene.light[i].att_range.y, deferredCmd_.scene.light[i].att_range.z);
			id = "Pos###Pos" + std::to_string(i);
			ImGui::DragFloat3(id.c_str(), (float*)& deferredCmd_.scene.light[i].pos, -100.f, 100.f);
			ImGui::Separator();
		}
	}
	ImGui::End();
}
void Scene::UpdateSceneTransform() {
	transform.pos += input.dpos;
	auto rot = glm::inverse(glm::mat3(camera_.view)) * input.drot;
	m = ScreenSpaceRotator({}, Transform{ transform.pos, transform.center, rot });
	auto irotm = glm::transpose(float3x3(m));
	for (auto& o : modoObjects_) {
		o.pos += input.dpos;
		o.rot = irotm * o.rot;
	}
}
void Scene::Update(double frame, double total) {
	assets_.Update(renderer_);
	if (state != State::Ready && assets_.status == assets::Assets::Status::kReady) {
		state = State::Ready;
		renderer_->Update(frame, total);
		return;
	}
	if (state != State::Ready) return;
	renderer_->Update(frame, total);
	ObjectsWindow();
	SceneWindow();
	for (const auto& o : objects_) {
		assert(assets_.models[o.mesh].layers.front().submeshes.size() <= 12);
	}
	camera_.Update();
	// need to determine it from view because of ScreenSpaceRotator...
	deferredCmd_.scene.eyePos = camera_.eyePos;
	// TODO:: WTF?
	deferredCmd_.scene.ip = glm::inverse(camera_.proj);
	deferredCmd_.scene.ivp = camera_.ivp;
	deferredCmd_.scene.nf.x = camera_.n; deferredCmd_.scene.nf.y = camera_.f;
	deferredCmd_.scene.viewport = { (float)viewport_.width, (float)viewport_.height };
	for (auto& o : objects_) {
		o.Update(frame, total);
	}
	// TODO:: update light pos here if neccessay
	for (int i = 0; i < MAX_LIGHTS; ++i)
		objects_[lights_[i].placeholder].pos = deferredCmd_.scene.light[i].pos;
}
