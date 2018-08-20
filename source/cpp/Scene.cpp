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
Scene::Object::Object(RendererWrapper* renderer, const Mesh& mesh, const SceneShaderResources& sceneShaderResources, bool debug) {
	using namespace ShaderStructures;
	layers.reserve(mesh.layers.size());
	for (auto& layer : mesh.layers) {
		layers.push_back({});
		Scene::Object::Layer& l = layers.back();
		l.pivot = layer.pivot;
		// pos
		l.cMVP = renderer->CreateShaderResource(sizeof(ShaderStructures::cMVP), ShaderStructures::cMVP::frame_count);
		// tex
		l.cObject = renderer->CreateShaderResource(sizeof(ShaderStructures::cObject), ShaderStructures::cObject::frame_count);
		for (auto& submesh : layer.submeshes) {
			if (debug) {
				// debug
				l.debugCmd.push_back({});
				auto& cmd = l.debugCmd.back();
				cmd.offset = submesh.offset; cmd.count = submesh.count;
				cmd.vb = mesh.vb; cmd.ib = mesh.ib;
#ifdef PLATFORM_WIN
				// cMVP
				uint16_t offset = 0;
				cmd.descAllocEntryIndex = renderer->AllocDescriptors(DebugCmd::Params::desc_count);
				auto rootParamIndex = DebugCmd::Params::index<cMVP>::value;
				// TODO:: root parameter index is redundant
				cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
				for (uint32_t frame = 0; frame < cMVP::frame_count; ++frame) {
					renderer->CreateCBV(cmd.descAllocEntryIndex, offset, frame, l.cMVP + frame);
				}
				++offset;
				// cColor
				renderer->CreateCBV(cmd.descAllocEntryIndex, offset, submesh.material.cColor);
				rootParamIndex = DebugCmd::Params::index<cColor>::value;
				cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
				++offset;
#elif defined(PLATFORM_MAC_OS)
				cmd.vsBuffers[DebugCmd::VSParams::index<cMVP>::value] = {l.cMVP, cMVP::frame_count};
				cmd.fsBuffers[DebugCmd::FSParams::index<cColor>::value] = {submesh.material.cColor, cColor::frame_count};
#endif
			} else if (submesh.material.tStaticColorTexture != InvalidBuffer) {
				// tex
				l.texCmd.push_back({});
				auto& cmd = l.texCmd.back();
				cmd.offset = submesh.offset; cmd.count = submesh.count;
				cmd.vb = mesh.vb; cmd.ib = mesh.ib; cmd.nb = mesh.nb; cmd.uvb = submesh.material.staticColorUV;
#ifdef PLATFORM_WIN
				cmd.descAllocEntryIndex = renderer->AllocDescriptors(TexCmd::Params::desc_count);
				// cObject
				uint16_t offset = 0;
				{
					// 1st root parameter for vs, 1st descriptor table with 1 descriptor entry
					auto rootParamIndex = TexCmd::Params::index<cObject>::value;
					cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
					for (uint32_t frame = 0; frame < cObject::frame_count; ++frame) {
						renderer->CreateCBV(cmd.descAllocEntryIndex, offset, frame, l.cObject + frame);
					}
					++offset;
				}

				{
					// 2nd root parameter for ps, 2nd descriptor table with 3 descriptor entry
					// tTexture
					renderer->CreateSRV(cmd.descAllocEntryIndex, offset, submesh.material.tStaticColorTexture);
					auto rootParamIndex = TexCmd::Params::index<tTexture<1>>::value;
					cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
					++offset;

					// cMaterial
					renderer->CreateCBV(cmd.descAllocEntryIndex, offset, submesh.material.cMaterial);
					++offset;
				}
#elif defined(PLATFORM_MAC_OS)
				cmd.vsBuffers[TexCmd::VSParams::index<cObject>::value] = {l.cObject, cObject::frame_count};
				cmd.textures[0] = submesh.material.tStaticColorTexture;
				cmd.fsBuffers[TexCmd::FSParams::index<cMaterial>::value] = {submesh.material.cMaterial, cMaterial::frame_count};
#endif
			} else {
				l.posCmd.push_back({});
				auto& cmd = l.posCmd.back();
				cmd.vb = mesh.vb; cmd.ib = mesh.ib; cmd.nb = mesh.nb;
				cmd.offset = submesh.offset; cmd.count = submesh.count;
#ifdef PLATFORM_WIN
				cmd.descAllocEntryIndex = renderer->AllocDescriptors(PosCmd::Params::desc_count);
				uint16_t offset = 0;
				{
					// 1st root parameter, 1st descriptor table with 1 descriptor entry
					auto rootParamIndex = PosCmd::Params::index<cObject>::value;
					cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
					for (uint32_t frame = 0; frame < cObject::frame_count; ++frame) {
						renderer->CreateCBV(cmd.descAllocEntryIndex, offset, frame, l.cObject + frame);
					}
					++offset;
				}

				{
					auto rootParamIndex = PosCmd::Params::index<cMaterial>::value;
					cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
					// cMaterial
					renderer->CreateCBV(cmd.descAllocEntryIndex, offset, submesh.material.cMaterial);
					++offset;
				}
#elif defined(PLATFORM_MAC_OS)
				cmd.vsBuffers[PosCmd::VSParams::index<cObject>::value] = {l.cObject, cObject::frame_count};
				cmd.fsBuffers[PosCmd::FSParams::index<cMaterial>::value] = {submesh.material.cMaterial, cMaterial::frame_count};
#endif
			}
		}
	}
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
	debug_.emplace_back(renderer_, assets_.staticModels[Assets::LIGHT], shaderResources, true);
	debug_.emplace_back(renderer_, assets_.staticModels[Assets::LIGHT], shaderResources, true);
	debug_.emplace_back(renderer_, assets_.staticModels[Assets::PLACEHOLDER], shaderResources, true);
	objects_.emplace_back(renderer_, assets_.staticModels[Assets::CHECKERBOARD], shaderResources, false);
	objects_.emplace_back(renderer_, assets_.staticModels[Assets::BEETHOVEN], shaderResources, false);
	// TODO:: remove
	objects_.back().pos += glm::vec3{ 0.f, .5f, 0.f };
	const float incX = 2.4f, incY = 2.9f;
	auto pos = glm::vec3{ -3 * incX, -3 * incY, -2.f };
	for (int i = 0; i < 7; ++i) {
		for (int j = 0; j < 7; ++j) {
			objects_.emplace_back(renderer_, assets_.staticModels[Assets::SPHERE], shaderResources, false);
			objects_.back().pos = pos;

			auto res = assets_.materials.emplace(L"mat_" + std::to_wstring(i) + L'_' + std::to_wstring(j), Material{});
			Material& gpuMaterial = res.first->second;
			gpuMaterial.cMaterial = renderer_->CreateShaderResource(sizeof(ShaderStructures::cMaterial), ShaderStructures::cMaterial::frame_count);
			ShaderStructures::cMaterial data;
			data.material.diffuse[0] = .8f; data.material.diffuse[1] = .0f; data.material.diffuse[2] = .0f;
			data.material.specular = glm::clamp(j / 6.f, 0.025f, 1.f);
			data.material.power = i / 6.f;
			auto& cmd = objects_.back().layers.front().posCmd.front();
#ifdef PLATFORM_WIN
			cmd.descAllocEntryIndex = renderer_->AllocDescriptors(ShaderStructures::PosCmd::Params::desc_count);
			uint16_t offset = 0;
			{
				// 1st root parameter, 1st descriptor table with 1 descriptor entry
				auto rootParamIndex = ShaderStructures::PosCmd::Params::index<ShaderStructures::cObject>::value;
				cmd.bindings[rootParamIndex] = ShaderStructures::ResourceBinding{ rootParamIndex, offset };
				for (uint32_t frame = 0; frame < ShaderStructures::cObject::frame_count; ++frame) {
					renderer_->CreateCBV(cmd.descAllocEntryIndex, offset, frame, objects_.back().layers.front().cObject + frame);
				}
				++offset;
			}

			{
				auto rootParamIndex = ShaderStructures::PosCmd::Params::index<ShaderStructures::cMaterial>::value;
				cmd.bindings[rootParamIndex] = ShaderStructures::ResourceBinding{ rootParamIndex, offset };
				// cMaterial
				renderer_->CreateCBV(cmd.descAllocEntryIndex, offset, gpuMaterial.cMaterial);
				++offset;
			}
#elif defined(PLATFORM_MAC_OS)
			cmd.fsBuffers[ShaderStructures::PosCmd::FSParams::index<ShaderStructures::cMaterial>::value] =
				{gpuMaterial.cMaterial, ShaderStructures::cMaterial::frame_count};
#endif
			renderer_->UpdateShaderResource(gpuMaterial.cMaterial, &data, sizeof(ShaderStructures::cMaterial));
			pos.x += incX;
		}
		pos.y += incY;
		pos.x = -3 * incX;
	}
	
	lights_[0].placeholder = &debug_[0];
	lights_[1].placeholder = &debug_[1];
	lights_[0].placeholder->pos = ToVec3(lights_[0].pointLight.pos);
	lights_[1].placeholder->pos = ToVec3(lights_[1].pointLight.pos);
	// TODO:: remove
	objects_.back().pos.y += .5f;
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

	// TODO:: remove
	m_radiansPerSecond = glm::quarter_pi<float>();
	m_angle = 0;
	m_tracking = false;
	// TODO:: remove 
}
void Scene::Render() {
	if (!loadCompleted) return;
	for (const auto& o : objects_)
		for (const auto& l : o.layers) {
			for (const auto& cmd : l.posCmd)
				renderer_->Submit(cmd);
			for (const auto& cmd : l.texCmd)
				renderer_->Submit(cmd);
		}
	for (const auto& o : debug_)
		for (const auto& l : o.layers) {
			for (const auto& cmd : l.debugCmd)
				renderer_->Submit(cmd);
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
	// TODO:: remove
	m_angle += static_cast<float>(total) * m_radiansPerSecond / 1000.f;
	// TODO:: remove

	camera_.Update();
	// need to determine it from view because of ScreenSpaceRotator...
	FromVec3(camera_.GetEyePos(), shaderStructures.cScene.scene.eyePos);
	auto ip = glm::inverse(camera_.proj);
	memcpy(shaderStructures.cScene.scene.ip, &ip, sizeof(shaderStructures.cScene.scene.ip));
	memcpy(shaderStructures.cScene.scene.ivp, &camera_.ivp, sizeof(shaderStructures.cScene.scene.ivp));
	shaderStructures.cScene.scene.n = camera_.n; shaderStructures.cScene.scene.f = camera_.f;
	for (int i = 0; i < MAX_LIGHTS; ++i) {
		shaderStructures.cScene.scene.light[i] = lights_[i].pointLight;
		//FromVec3(camera_.view * glm::vec4(ToVec3(lights_[i].pointLight.pos), 1.f), shaderStructures.cScene.scene.light[i].pos);
	}

	renderer_->UpdateShaderResource(shaderResources.cScene + renderer_->GetCurrenFrameIndex(), &shaderStructures.cScene, sizeof(shaderStructures.cScene));
	for (auto& o : objects_) {
		o.Update(frame, total);
		for (const auto& layer : o.layers) {
			auto m = this->m * glm::translate(RotationMatrix(o.rot.x, o.rot.y, o.rot.z), o.pos);
			m[3] += glm::vec4(layer.pivot, 0.f);
			auto mvp = glm::transpose(camera_.vp * m);
			// TODO:: update resources according to selected encoderIndex/shaderid
			ShaderStructures::cMVP cMVP;
			memcpy(cMVP.mvp, &mvp, sizeof(cMVP.mvp));
			renderer_->UpdateShaderResource(layer.cMVP + renderer_->GetCurrenFrameIndex(), &cMVP, sizeof(cMVP));

			ShaderStructures::cObject cObject;
			memcpy(cObject.obj.mvp, &mvp, sizeof(cObject.obj.mvp));
			m = glm::transpose(m);
			memcpy(cObject.obj.m, &m, sizeof(cObject.obj.m));
			renderer_->UpdateShaderResource(layer.cObject + renderer_->GetCurrenFrameIndex(), &cObject, sizeof(cObject));
		}
	}
	// TODO:: update light pos here if neccessay
	lights_[0].placeholder->pos = ToVec3(lights_[0].pointLight.pos);
	lights_[1].placeholder->pos = ToVec3(lights_[1].pointLight.pos);
	for (auto& o : debug_) {
		o.Update(frame, total);
		for (const auto& layer : o.layers) {
			auto m = glm::translate(RotationMatrix(o.rot.x, o.rot.y, o.rot.z), o.pos);
			m[3] += glm::vec4(layer.pivot, 0.f);
			auto mvp = glm::transpose(camera_.vp * m);
			// TODO:: update resources according to selected encoderIndex/shaderid
			ShaderStructures::cMVP cMVP;
			memcpy(cMVP.mvp, &mvp, sizeof(cMVP.mvp));
			renderer_->UpdateShaderResource(layer.cMVP + renderer_->GetCurrenFrameIndex(), &cMVP, sizeof(cMVP));

			ShaderStructures::cObject cObject;
			memcpy(cObject.obj.mvp, &mvp, sizeof(cObject.obj.mvp));
			m = glm::transpose(m);
			memcpy(cObject.obj.m, &m, sizeof(cObject.obj.m));
			renderer_->UpdateShaderResource(layer.cObject + renderer_->GetCurrenFrameIndex(), &cObject, sizeof(cObject));
		}
	}
}
