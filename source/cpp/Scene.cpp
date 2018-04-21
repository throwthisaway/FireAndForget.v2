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
		l.cMVP = renderer->CreateShaderResource(sizeof(ShaderStructures::cMVP), RendererWrapper::frameCount_);
		// tex
		l.cObject = renderer->CreateShaderResource(sizeof(ShaderStructures::cObject), RendererWrapper::frameCount_);
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
				cmd.descAllocEntryIndex = renderer->AllocDescriptors(DebugCmd::Params::count);
				auto rootParamIndex = DebugCmd::Params::index<cMVP>::value;
				// TODO:: root parameter index is redundant
				cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
				for (uint32_t frame = 0; frame < cMVP::numDesc; ++frame) {
					renderer->CreateCBV(cmd.descAllocEntryIndex, offset, frame, l.cMVP + frame);
				}
				++offset;
				// cColor
				renderer->CreateCBV(cmd.descAllocEntryIndex, offset, submesh.material.cColor);
				rootParamIndex = DebugCmd::Params::index<cColor>::value;
				cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
				++offset;
#elif defined(PLATFORM_MAC_OS)
				cmd.vsBuffers[DebugCmd::VSParams::index<cMVP>::value] = {l.cMVP, cMVP::numDesc};
				cmd.fsBuffers[DebugCmd::FSParams::index<cColor>::value] = {submesh.material.cColor, cColor::numDesc};
#endif
			} else if (submesh.material.tStaticColorTexture != InvalidBuffer) {
				// tex
				l.texCmd.push_back({});
				auto& cmd = l.texCmd.back();
				cmd.offset = submesh.offset; cmd.count = submesh.count;
				cmd.vb = mesh.vb; cmd.ib = mesh.ib; cmd.nb = mesh.nb; cmd.uvb = submesh.material.staticColorUV;
#ifdef PLATFORM_WIN
				cmd.descAllocEntryIndex = renderer->AllocDescriptors(TexCmd::Params::count);
				// cObject
				uint16_t offset = 0;
				{
					// 1st root parameter, 1st descriptor table with 1 descriptor entry
					auto rootParamIndex = TexCmd::Params::index<cObject>::value;
					cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
					for (uint32_t frame = 0; frame < cObject::numDesc; ++frame) {
						renderer->CreateCBV(cmd.descAllocEntryIndex, offset, frame, l.cObject + frame);
					}
					++offset;
				}

				{
					// 2nd root parameter, 2nd descriptor table with 3 descriptor entry
					// tTexture
					renderer->CreateSRV(cmd.descAllocEntryIndex, offset, submesh.material.tStaticColorTexture);
					auto rootParamIndex = TexCmd::Params::index<tTexture>::value;
					cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
					++offset;

					// cMaterial
					renderer->CreateCBV(cmd.descAllocEntryIndex, offset, submesh.material.cMaterial);
					++offset;

					// cScene
					for (uint32_t frame = 0; frame < cScene::numDesc; ++frame) {
						renderer->CreateCBV(cmd.descAllocEntryIndex, offset, frame, sceneShaderResources.cScene + frame);
					}
					++offset;
				}
#elif defined(PLATFORM_MAC_OS)
				cmd.vsBuffers[TexCmd::VSParams::index<cObject>::value] = {l.cObject, cObject::numDesc};
				cmd.textures[0] = submesh.material.tStaticColorTexture;
				cmd.fsBuffers[TexCmd::FSParams::index<cMaterial>::value] = {submesh.material.cMaterial, cMaterial::numDesc};
				cmd.fsBuffers[TexCmd::FSParams::index<cScene>::value] = {sceneShaderResources.cScene, cScene::numDesc};
#endif
			} else {
				l.posCmd.push_back({});
				auto& cmd = l.posCmd.back();
				cmd.vb = mesh.vb; cmd.ib = mesh.ib; cmd.nb = mesh.nb;
				cmd.offset = submesh.offset; cmd.count = submesh.count;
#ifdef PLATFORM_WIN
				cmd.descAllocEntryIndex = renderer->AllocDescriptors(PosCmd::Params::count);
				uint16_t offset = 0;
				{
					// 1st root parameter, 1st descriptor table with 1 descriptor entry
					auto rootParamIndex = PosCmd::Params::index<cObject>::value;
					cmd.bindings[rootParamIndex] = ResourceBinding{ rootParamIndex, offset };
					for (uint32_t frame = 0; frame < cObject::numDesc; ++frame) {
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

					// cScene
					for (uint32_t frame = 0; frame < cScene::numDesc; ++frame) {
						renderer->CreateCBV(cmd.descAllocEntryIndex, offset, frame, sceneShaderResources.cScene + frame);
					}
					++offset;
				}
#elif defined(PLATFORM_MAC_OS)
				cmd.vsBuffers[PosCmd::VSParams::index<cObject>::value] = {l.cObject, cObject::numDesc};
				cmd.fsBuffers[PosCmd::FSParams::index<cMaterial>::value] = {submesh.material.cMaterial, cMaterial::numDesc};
				cmd.fsBuffers[PosCmd::FSParams::index<cScene>::value] = {sceneShaderResources.cScene, cScene::numDesc};
#endif
			}
		}
	}
}
namespace {
static const float defaultLightRange = 25.f;
static const ShaderStructures::PointLight defaultLight = { {.4f, .4f, .4f} ,{}/* diffuse */,
	{.0f, .0f, .0f}, {} /* ambient */,
	{.8f, .8f, .8f}, {} /* specular highlight */,
	{ 2.f, 2.f, 2.f },{} /* position */,
	{ 1.f, 2.f / defaultLightRange, 1.f / (defaultLightRange * defaultLightRange) } /* attenuation */, {},
	defaultLightRange /* range */ };
}
void Scene::OnAssetsLoaded() {
	debug_.emplace_back(renderer_, assets_.staticModels[Assets::LIGHT], shaderResources, true);
	debug_.emplace_back(renderer_, assets_.staticModels[Assets::LIGHT], shaderResources, true);
	debug_.emplace_back(renderer_, assets_.staticModels[Assets::PLACEHOLDER], shaderResources, true);
	objects_.emplace_back(renderer_, assets_.staticModels[Assets::CHECKERBOARD], shaderResources, false);
	objects_.emplace_back(renderer_, assets_.staticModels[Assets::BEETHOVEN], shaderResources, false);
	// TODO:: remove
	objects_.back().pos += glm::vec3{0.f, .5f, 0.f};

	lights_[0] = &debug_[0];
	lights_[1] = &debug_[1];
	lights_[0]->pos = ToVec3(shaderStructures.cScene.light[0].pos);
	lights_[1]->pos = ToVec3(shaderStructures.cScene.light[1].pos);
	// TODO:: remove
	objects_.back().pos.y += .5f;
	loadCompleted = true;
}
void Scene::Init(RendererWrapper* renderer, int width, int height) {
	renderer_ = renderer;
	camera_.Perspective(width, height);
	
	const float Z = -1.5f;
	camera_.transform.pos = { 0.f, 0.f, Z };
	camera_.view = ScreenSpaceRotator({}, camera_.transform);
	FromVec3(camera_.transform.pos, shaderStructures.cScene.eyePos);
	for (int i = 0; i < sizeof(shaderStructures.cScene.light) / sizeof(shaderStructures.cScene.light[0]); ++i) {
		shaderStructures.cScene.light[i] = defaultLight;
	}
	shaderStructures.cScene.light[0].pos[0] = -2.f;

	shaderResources.cScene = renderer->CreateShaderResource(sizeof(ShaderStructures::cScene), ShaderStructures::cScene::numDesc);
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
	camera_.transform.pos += input.dpos;
	camera_.transform.rot += input.drot;
	camera_.view = ScreenSpaceRotator(camera_.view, Transform{ camera_.transform.pos, camera_.transform.center, input.drot});
}

void Scene::UpdateSceneTransform() {
	transform.pos += input.dpos;
	auto rot = glm::inverse(glm::mat3(camera_.view)) * input.drot;
	transform.rot += rot;
	m = ScreenSpaceRotator(m, Transform{ transform.pos, transform.center, rot });
}
void Scene::Update(double frame, double total) {
	if (!loadCompleted) return;
	// TODO:: remove
	m_angle += static_cast<float>(total) * m_radiansPerSecond / 1000.f;
	// TODO:: remove

	camera_.Update();
	FromVec3(camera_.transform.pos, shaderStructures.cScene.eyePos);
	renderer_->UpdateShaderResource(shaderResources.cScene + renderer_->GetCurrenFrameIndex(), &shaderStructures.cScene, sizeof(shaderStructures.cScene));
	for (auto& o : objects_) {
		o.Update(frame, total);
		for (const auto& layer : o.layers) {
			auto m = this->m * glm::translate(RotationMatrix(glm::translate(glm::mat4{}, layer.pivot), o.rot.x, o.rot.y, o.rot.z), o.pos);
			auto mvp = glm::transpose(camera_.vp * m);
			// TODO:: update resources according to selected encoderIndex/shaderid
			ShaderStructures::cMVP cMVP;
			memcpy(cMVP.mvp, &mvp, sizeof(cMVP.mvp));
			renderer_->UpdateShaderResource(layer.cMVP + renderer_->GetCurrenFrameIndex(), &cMVP, sizeof(cMVP));

			ShaderStructures::cObject cObject;
			memcpy(cObject.mvp, &mvp, sizeof(cObject.mvp));
			memcpy(cObject.m, &m, sizeof(cObject.m));
			renderer_->UpdateShaderResource(layer.cObject + renderer_->GetCurrenFrameIndex(), &cObject, sizeof(cObject));
		}
	}
	// TODO:: update light pos here if neccessay
	lights_[0]->pos = ToVec3(shaderStructures.cScene.light[0].pos);
	lights_[1]->pos = ToVec3(shaderStructures.cScene.light[1].pos);
	for (auto& o : debug_) {
		o.Update(frame, total);
		for (const auto& layer : o.layers) {
			auto m = glm::translate(RotationMatrix(glm::translate(glm::mat4{}, layer.pivot), o.rot.x, o.rot.y, o.rot.z), o.pos);
			auto mvp = glm::transpose(camera_.vp * m);
			// TODO:: update resources according to selected encoderIndex/shaderid
			ShaderStructures::cMVP cMVP;
			memcpy(cMVP.mvp, &mvp, sizeof(cMVP.mvp));
			renderer_->UpdateShaderResource(layer.cMVP + renderer_->GetCurrenFrameIndex(), &cMVP, sizeof(cMVP));

			ShaderStructures::cObject cObject;
			memcpy(cObject.mvp, &mvp, sizeof(cObject.mvp));
			memcpy(cObject.m, &m, sizeof(cObject.m));
			renderer_->UpdateShaderResource(layer.cObject + renderer_->GetCurrenFrameIndex(), &cObject, sizeof(cObject));
		}
	}
}
