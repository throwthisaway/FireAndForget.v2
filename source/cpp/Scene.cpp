#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"
#include "imgui.h"
#include "DebugUI.h"

// TODO::
// consolidate sampler names
// investigate moire pattern on light specular
// fix modo export rh->lh, uv bottom-left->top left orientation
// generate and sample mips
// merge Fraginput.h.metal with ShaderInput.hlsli
// WorldPosFromDepth->ViewPosFromDepth is possible???
// WIN: remove texSSAODebug
// WIN: genkernel
// WIN: ??? float distZ = abs(r.z - p.z);
// WIN: linearSmp to linearclampsmp in DeferredPBR
// MAC: changed file format (Header, vertextype in submesh, only shadow test regenerated)
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
	ShaderId SelectShadowShader(uint32_t vertexType) {
		switch(vertexType) {
			case 0:
				break;
			case (1 << (int)ModoMeshLoader::VertexFields::kNormal):
				return ShaderStructures::ShadowPos;
			case (1 << (int)ModoMeshLoader::VertexFields::kNormal) |
					(1 << (int)ModoMeshLoader::VertexFields::kTangent) |
					(1 << (int)ModoMeshLoader::VertexFields::kUV0) :
					return ShaderStructures::ShadowModoDN;
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
	float y = -0.25f;// -.5f * (assets_.meshes.size() >> 1);
	for (int i = assets::Assets::STATIC_MODEL_COUNT; i < assets_.meshes.size(); ++i) {
//		auto& mesh = assets_.meshes[i];
		modoObjects_.push_back({{ 0.f, y, 0.f}, {0.f, glm::pi<float>(), 0.f}, (index_t)i});
		y += 1.5f;
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

	for (const auto& o : modoObjects_) {
		const auto& mesh = assets_.meshes[o.mesh];
		if (r < mesh.r) r = mesh.r;
		aabb.Add(mesh.aabb);
	}

	ssaoCmd_.ao.bias = .1f;
	ssaoCmd_.ao.power = 4.f;
	ssaoCmd_.ao.rad = .04f;
	ssaoCmd_.ao.scale = 1.f;
	ssaoCmd_.ao.intensity = 1.f;
	ssaoCmd_.ao.fadeStart = .1f;
	ssaoCmd_.ao.fadeEnd = .1f;

	for (int i = 0; i < MAX_LIGHTS; ++i) {
		deferredCmd_.scene.light[i] = defaultPointLight;
		//FromVec3(camera_.view * glm::vec4(ToVec3(lights_[i].pointLight.pos), 1.f), shaderStructures.cScene.scene.light[i].pos);
	}
	deferredCmd_.scene.light[0].pos.x = deferredCmd_.scene.light[0].pos.y = -4.f;
	renderer_->BeginPrePass();
	ssaoCmd_.random = deferredCmd_.random = assets_.textures[assets::Assets::RANDOM];
	Dim dim = renderer_->GetDimensions(deferredCmd_.random);
	ssaoCmd_.ao.randomFactor.x = (float)(viewport_.width >> 1) / (float)dim.w;
	ssaoCmd_.ao.randomFactor.y = (float)(viewport_.height >> 1) / (float)dim.h;
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

	{
		const uint32_t shadowMapDim = 512;
		shadowMaps_[0].width = shadowMaps_[0].height = shadowMapDim;
		shadowMaps_[0].dir = glm::normalize(float3(.3, -.5f, 1.f));
		float dim = r;
		shadowMaps_[0].vp = glm::orthoLH_ZO(-dim, dim, -dim, dim, .1f, 100.f) * glm::lookAtLH(-shadowMaps_[0].dir * r, shadowMaps_[0].dir, float3(0.f, 0.f, 1.f));
		shadowMaps_[0].rt = renderer_->CreateShadowRT(shadowMaps_[0].width, shadowMaps_[0].height);
	}
	state = State::Ready;
}
void Scene::Init(Renderer* renderer, int width, int height) {
	state = State::Start;
	m = glm::identity<glm::mat4x4>();
	using namespace ShaderStructures;
	renderer_ = renderer;
	OnResize(width, height);

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
	for (int i = 0; i < MAX_SHADOWMAPS; ++i) {
		renderer_->StartShadowPass(shadowMaps_[i].rt);
		for (const auto& o : modoObjects_) {
			const auto& mesh = assets_.meshes[o.mesh];
			auto m = this->m * glm::translate(RotationMatrix(o.rot.x, o.rot.y, o.rot.z), o.pos);
			//m[3] += float4(l.pivot, 0.f);
			auto mvp = shadowMaps_[i].vp * m;
			for (const auto& submesh : mesh.submeshes) {
				ShaderId shader = SelectShadowShader(submesh.vertexType);
				ShaderStructures::ShadowCmd cmd{ mvp, submesh, mesh.vb, mesh.ib, shader};
				renderer_->ShadowPass(cmd);
			}
		}
		renderer_->EndShadowPass(shadowMaps_[i].rt);
	}
	renderer_->StartGeometryPass();
	//for (const auto& o : objects_) {
	//	assert(assets_.models[o.mesh].layers.front().submeshes.size() <= 12);
	//	const auto& mesh = assets_.models[o.mesh];
	//	for (const auto& l : mesh.layers) {
	//		auto m = this->m * glm::translate(RotationMatrix(o.rot.x, o.rot.y, o.rot.z), o.pos);
	//		m[3] += float4(l.pivot, 0.f);
	//		auto mvp = camera_.vp * m;
	//		for (const auto& submesh : l.submeshes) {
	//			const auto& material = assets_.materials[submesh.material];
	//			ShaderId shader = (submesh.texAlbedo != InvalidTexture && submesh.vertexType == VertexType::PNT) ? ShaderStructures::Tex : ShaderStructures::Pos;
	//			ShaderStructures::DrawCmd cmd{ m, mvp, submesh, material, mesh.vb, mesh.ib, shader};
	//			renderer_->Submit(cmd);
	//		}
	//	}
	//}

	for (const auto& o : modoObjects_) {
		const auto& mesh = assets_.meshes[o.mesh];
		auto m = this->m * glm::translate(RotationMatrix(o.rot.x, o.rot.y, o.rot.z), o.pos);
		//m[3] += float4(l.pivot, 0.f);
		auto mvp = camera_.vp * m;
		auto mv = camera_.view * m;
		for (const auto& submesh : mesh.submeshes) {
			ShaderId shader = SelectModoShader(submesh.uvCount, submesh.textureMask);
			ShaderStructures::ModoDrawCmd cmd{ {mvp, m, mv}, submesh, submesh.material, mesh.vb, mesh.ib, shader };
			renderer_->Submit(cmd);
		}
	}
	renderer_->SSAOPass(ssaoCmd_);
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
		ImGui::Text("%s: %f %f %f | %f %f %f", assets_.meshes[o.mesh].name.c_str(), o.pos.x, o.pos.y, o.pos.z, o.rot.x, o.rot.y, o.rot.z);
		for (auto& s : assets_.meshes[o.mesh].submeshes) {
			if ((s.textureMask & (1 << (int)ModoMeshLoader::TextureTypes::kAlbedo)) == 0) {
				std::string idDiffuse("Diffuse###MaterialDiffuse" + std::to_string(++id));
				ImGui::ColorEdit3(idDiffuse.c_str(), (float*)& s.material.diffuse);
			}
			if ((s.textureMask & (1 << (int)ModoMeshLoader::TextureTypes::kMetallic)) == 0) {
				auto idMetallic = std::string("Metallic###Metallic") + std::to_string(++id);
				ImGui::SliderFloat(idMetallic.c_str(), &s.material.metallic_roughness.x, 0.f, 1.f);
			}
			if ((s.textureMask & (1 << (int)ModoMeshLoader::TextureTypes::kRoughness)) == 0) {
				auto idRoughness = std::string("Roughness###Roughness1") + std::to_string(++id);
				ImGui::SliderFloat(idRoughness.c_str(), &s.material.metallic_roughness.y, 0.05f, 1.f);
			}
			ImGui::Separator();
		}
	}
	ImGui::End();
}
void Scene::SceneWindow() {
	ImGui::SetNextWindowPos(ImVec2(20, 600), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Scene Window");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	if (ImGui::CollapsingHeader("Camera", &ui.cameraOpen)) {
		ImGui::DragFloat3("Pos###PosCamera", (float*)& camera_.pos, 0.f, 100.f);
		ImGui::DragFloat3("Rot###RotCamera", (float*)& camera_.rot, -glm::pi<float>(), glm::pi<float>());
		ImGui::DragFloat3("EyePos", (float*)& camera_.eyePos, 0.f, 100.f);
	}
	if (ImGui::CollapsingHeader("AO", &ui.aoOpen)) {
		ImGui::SliderFloat("Intensity", &ssaoCmd_.ao.intensity, 0.f, 1.f);
		ImGui::SliderFloat("Radius", &ssaoCmd_.ao.rad, 0.f, .1f);
		ImGui::SliderFloat("Scale", &ssaoCmd_.ao.scale, 0.f, 2.f);
		ImGui::SliderFloat("Fade start", &ssaoCmd_.ao.fadeStart, 0.f, .2f);
		ImGui::SliderFloat("Fade end", &ssaoCmd_.ao.fadeEnd, 0.f, .2f);
		ImGui::SliderFloat("Bias", &ssaoCmd_.ao.bias, 0.001f, .1f);
		ImGui::SliderFloat("Power", &ssaoCmd_.ao.power, 0.1f, 7.f);
	}
	if (ImGui::CollapsingHeader("Lights", &ui.lightOpen)) {
		for (int i = 0; i < MAX_LIGHTS; ++i) {
			std::string id("Diffuse###LightDiffuse" + std::to_string(i));
			ImGui::DragFloat3(id.c_str(), (float*)&deferredCmd_.scene.light[i].diffuse, 0.f, 1000.f);
			id = "Specular###LightSpecular" + std::to_string(i);
			ImGui::ColorEdit3(id.c_str(), (float*)&deferredCmd_.scene.light[i].specular);
			id = "Ambient###LightAmbient" + std::to_string(i);
			ImGui::ColorEdit3(id.c_str(), (float*)&deferredCmd_.scene.light[i].ambient);
			id = "Range###LightRange" + std::to_string(i);
			ImGui::SliderFloat(id.c_str(), &deferredCmd_.scene.light[i].att_range.w, 1.f, 500.f);
			deferredCmd_.scene.light[i].att_range = float4(CalcAtt(deferredCmd_.scene.light[i].att_range.w), deferredCmd_.scene.light[i].att_range.w);
			ImGui::Text("Attenuation: %f %f %f", deferredCmd_.scene.light[i].att_range.x, deferredCmd_.scene.light[i].att_range.y, deferredCmd_.scene.light[i].att_range.z);
			id = "Pos###LightPos" + std::to_string(i);
			ImGui::DragFloat3(id.c_str(), (float*)& deferredCmd_.scene.light[i].pos, -100.f, 100.f);
			ImGui::Separator();
		}
	}
	ImGui::End();
}
//static glm::vec3 rot;
void Scene::UpdateSceneTransform() {
	transform.pos += input.dpos;
	auto rot = glm::inverse(glm::mat3(camera_.view)) * input.drot;
	m = ScreenSpaceRotator(m, Transform{ transform.pos, transform.center, rot });
	auto irotm = glm::transpose(float3x3(m));
	/*for (auto& o : modoObjects_) {
		o.pos += input.dpos;
		o.rot = irotm * o.rot;
		::rot = o.rot;
	}*/
}
void Scene::OnResize(int width, int height) {
	viewport_.width = width; viewport_.height = height;
	camera_.Perspective(width, height);
}
void Scene::Update(double frame, double total) {
	assets_.Update(renderer_);
	if (state != State::Ready && assets_.status == assets::Assets::Status::kReady && renderer_->Ready()) {
		state = State::Ready;
		renderer_->Update(frame, total);
		return;
	}
	if (state != State::Ready) return;
	renderer_->Update(frame, total);
	DEBUGUI(ObjectsWindow());
	DEBUGUI(SceneWindow());
	/*ImGui::Begin("Rot Window");
	ImGui::Text("%5.5g %5.5g %5.5g", rot.x, rot.y, rot.z);
	ImGui::End();*/
	for (const auto& o : objects_) {
		assert(assets_.models[o.mesh].layers.front().submeshes.size() <= 12);
	}
	camera_.Update();
	ssaoCmd_.ip = camera_.ip;
	ssaoCmd_.scene.proj = camera_.proj;
	ssaoCmd_.scene.ip = camera_.ip;
	ssaoCmd_.scene.ivp = camera_.ivp;
	ssaoCmd_.scene.view = camera_.view;
	ssaoCmd_.scene.viewport = { (float)viewport_.width, (float)viewport_.height };

	// need to determine it from view because of ScreenSpaceRotator...
	deferredCmd_.scene.eyePos = camera_.eyePos;
	// TODO:: WTF?
	deferredCmd_.scene.ip = camera_.ip;
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
