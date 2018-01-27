#include "pch.h"
#include "Scene.hpp"
#include "MatrixUtils.h"
#include "CreateShaderParams.h"

void Scene::Object::Update(double frame, double total) {
	// TODO:: currently everything is in Scene::Update
}
Scene::Object::Object(RendererWrapper* renderer, const Mesh& mesh, const SceneShaderResources& sceneShaderResources) {
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
			if (submesh.material.tStaticColorTexture != InvalidBuffer) {
				// tex
				l.texCmd.push_back({});
				auto& cmd = l.texCmd.back();
				cmd.offset = submesh.offset; cmd.count = submesh.count;
				cmd.vb = mesh.vb; cmd.ib = mesh.ib; cmd.nb = mesh.nb; cmd.uvb = submesh.material.staticColorUV;
				cmd.descAllocEntryIndex = renderer->AllocDescriptors(ShaderStructures::TexParams::numDesc::value);
				// TODO:: remove
				assert(ShaderStructures::TexParams::numDesc::value == 8);
				// cObject
				uint16_t offset = 0, count = ShaderStructures::cObject::numDesc;
				auto rootParamIndex = ShaderStructures::TexParams::index<ShaderStructures::cObject>::value;
				cmd.bindings[rootParamIndex] = ShaderStructures::ResourceBinding{ rootParamIndex, offset, count };
				for (; offset < count; ++offset) {
					renderer->CreateCBV(cmd.descAllocEntryIndex, offset, l.cObject);
				}

				// tTexture
				renderer->CreateSRV(cmd.descAllocEntryIndex, offset, submesh.material.tStaticColorTexture);
				count = ShaderStructures::tTexture::numDesc;
				rootParamIndex = ShaderStructures::TexParams::index<ShaderStructures::tTexture>::value;
				cmd.bindings[rootParamIndex] = ShaderStructures::ResourceBinding{ rootParamIndex, offset, count };
				offset += count;

				// cMaterial
				renderer->CreateCBV(cmd.descAllocEntryIndex, offset, submesh.material.cMaterial);
				count = ShaderStructures::cMaterial::numDesc;
				rootParamIndex = ShaderStructures::TexParams::index<ShaderStructures::cMaterial>::value;
				cmd.bindings[rootParamIndex] = ShaderStructures::ResourceBinding{ rootParamIndex, offset, count };
				offset += count;

				// cScene
				count = ShaderStructures::cScene::numDesc;
				for (; offset < count; ++offset) {
					renderer->CreateCBV(cmd.descAllocEntryIndex, offset, sceneShaderResources.cScene);
				}
				rootParamIndex = ShaderStructures::TexParams::index<ShaderStructures::cScene>::value;
				cmd.bindings[rootParamIndex] = ShaderStructures::ResourceBinding{ rootParamIndex, offset, count };
				offset += count;
			} else {
				// pos
				l.posCmd.push_back({});
				auto& cmd = l.posCmd.back();
				cmd.offset = submesh.offset; cmd.count = submesh.count;
				cmd.vb = mesh.vb; cmd.ib = mesh.ib; 
				// cMVP
				uint16_t offset = 0;
				uint16_t count = ShaderStructures::cMVP::numDesc;
				cmd.descAllocEntryIndex = renderer->AllocDescriptors(ShaderStructures::PosParams::numDesc::value);
				auto rootParamIndex = ShaderStructures::PosParams::index<ShaderStructures::cMVP>::value;
				// TODO:: root parameter index is redundant
				cmd.bindings[rootParamIndex] = ShaderStructures::ResourceBinding{ rootParamIndex, offset, count };
				// TODO:: remove
				assert(ShaderStructures::PosParams::numDesc::value == 4);
				for (; offset < count; ++offset) {
					renderer->CreateCBV(cmd.descAllocEntryIndex, offset, l.cMVP);
				}

				// cColor
				renderer->CreateCBV(cmd.descAllocEntryIndex, offset, submesh.material.cColor);
				count = ShaderStructures::cColor::numDesc;
				rootParamIndex = ShaderStructures::PosParams::index<ShaderStructures::cColor>::value;
				cmd.bindings[rootParamIndex] = ShaderStructures::ResourceBinding{ rootParamIndex, offset, count };
				offset += count;
			}
		}
	}
}
namespace {
static const float defaultLightRange = 25.f;
static const ShaderStructures::PointLight defaultLight = { {.6f, .6f, .6f} /* diffuse */,
	{.2f, .2f, .2f} /* ambient */,
	{.8f, .8f, .8f} /* specular highlight */,
	{1.f, 1.f, 1.f} /* position */, 
	{ 1.f, 2.f / defaultLightRange, 1.f / (defaultLightRange * defaultLightRange)} /* attenuation */,
	defaultLightRange /* range */};
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
	shaderStructures.cScene.eyePos[0] = camera_.pos.x; shaderStructures.cScene.eyePos[1] = camera_.pos.y; shaderStructures.cScene.eyePos[2] = camera_.pos.z;
	for (int i = 0; i < sizeof(shaderStructures.cScene.light) / sizeof(shaderStructures.cScene.light[0]); ++i) {
		shaderStructures.cScene.light[i] = defaultLight;
	}
	shaderResources.cScene = renderer->CreateShaderResource(sizeof(ShaderStructures::cScene), ShaderStructures::cScene::numDesc);
	assets_.Init(renderer);
	assets_.loadCompleteTask.then([this, renderer](Concurrency::task<void>& assetsWhenAllCompletion) {
		assetsWhenAllCompletion.then([this, renderer]() {
			objects_.emplace_back(renderer, assets_.staticModels[Assets::CHECKERBOARD], shaderResources);
			objects_.emplace_back(renderer, assets_.staticModels[Assets::BEETHOVEN], shaderResources);
			loadCompleted = true; });
	});
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
}
void Scene::Update(double frame, double total) {
	// TODO:: remove
	m_angle += static_cast<float>(total) * m_radiansPerSecond / 1000.f;
	// TODO:: remove

	camera_.Update();
	// TODO:: update cScene
	for (auto& o : objects_) {
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
