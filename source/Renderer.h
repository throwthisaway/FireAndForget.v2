#pragma once
#ifdef __OBJC__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import "Shaders.h"
#include "CBFrameAlloc.hpp"
#endif
#include "cpp/RendererTypes.h"
#include "cpp/ShaderStructures.h"
#include "Img.h"
struct Mesh;

struct Dim {
	uint32_t w, h;
};
class Renderer {
public:
#ifdef __OBJC__
	void BeginRender(id<MTLTexture> _Nonnull surface);
	void SetDrawable(id<CAMetalDrawable> _Nonnull drawable);
	void Init(id<MTLDevice> _Nonnull device, MTLPixelFormat pixelFormat);
#endif
	void CreateDeviceDependentResources();
	void CreateWindowSizeDependentResources();
	uint32_t GetCurrenFrameIndex() const;

	void BeginUploadResources();
	BufferIndex CreateBuffer(const void* _Nonnull buffer, size_t length);
	BufferIndex CreateBuffer(size_t length);
	TextureIndex CreateTexture(const void* _Nonnull buffer, uint64_t width, uint32_t height, Img::PixelFormat format, const char* _Nullable label = nullptr);
	Dim GetDimensions(TextureIndex);
	void EndUploadResources();

	void BeginRender();
	void BeginPrePass();
	TextureIndex GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint32_t dim, ShaderId shader, bool mip, const char* _Nullable label = nullptr);
	TextureIndex GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint32_t dim, ShaderId shader, const char* _Nullable label = nullptr);
	TextureIndex GenBRDFLUT(uint32_t dim, ShaderId shader, const char* _Nullable label = nullptr);
	void EndPrePass();
	void StartForwardPass();
	void Submit(const ShaderStructures::BgCmd& cmd);
	void StartGeometryPass();
	void Submit(const ShaderStructures::DrawCmd& cmd);
	void Submit(const ShaderStructures::ModoDrawCmd& cmd);
	void DoLightingPass(const ShaderStructures::DeferredCmd& cmd);
	void Render();
private:
#ifdef __OBJC__
	void MakeColorAttachmentTextures(NSUInteger width, NSUInteger height);
	void MakeDepthTexture(NSUInteger width, NSUInteger height);
	void Downsample(id<MTLCommandBuffer> _Nonnull commandBuffer,
				 id<MTLRenderPipelineState> _Nonnull pipelineState,
				 id<MTLTexture> _Nonnull srcTex,
				 id<MTLTexture> _Nonnull dstTex);
	id<MTLDevice> _Nonnull device_;
	id<MTLCommandQueue> _Nonnull commandQueue_;
	id<MTLTexture> _Nonnull depthTextures_[ShaderStructures::FrameCount], halfResDepthTextures_[ShaderStructures::FrameCount];
	id<MTLTexture> _Nonnull colorAttachmentTextures_[ShaderStructures::FrameCount][ShaderStructures::RenderTargetCount];
	std::vector<id<MTLBuffer>> buffers_;
	struct Texture {
		id<MTLTexture> _Nonnull texture;
		uint64_t width;
		uint32_t height;
		uint8_t bytesPerPixel;
		MTLPixelFormat format;

	};
	std::vector<Texture> textures_;
	std::vector<id<MTLRenderCommandEncoder>> encoders_;
	std::vector<id<MTLCommandBuffer>> commandBuffers_;
	Shaders* _Nonnull shaders_;
	id<MTLDepthStencilState> _Nonnull depthStencilState_;

	id<MTLSamplerState> _Nonnull defaultSamplerState_, deferredSamplerState_, mipmapSamplerState_, clampSamplerState_;
	id<MTLBuffer> _Nonnull fullscreenTexturedQuad_, cubeViews_;
	int cubeViewBufInc_;
	id<MTLTexture> _Nonnull deferredDebugColorAttachments_[ShaderStructures::FrameCount];

	dispatch_semaphore_t _Nonnull frameBoundarySemaphore_;
	uint32_t currentFrameIndex_ = 0;
	id<CAMetalDrawable> drawable_;
	id<MTLCommandBuffer> deferredCommandBuffer_;

	CBFrameAlloc frame_[ShaderStructures::FrameCount];
#endif
};
