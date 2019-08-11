#import "Renderer.h"
//#include <simd/vector_types.h>
#include "cpp/Assets.hpp"
#include "cpp/BufferUtils.h"
#include <glm/gtc/matrix_transform.hpp>
//#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

using namespace ShaderStructures;
namespace {
	constexpr size_t defaultCBFrameAllocSize = 65536;
	MTLPixelFormat PixelFormatToMTLPixelFormat(Img::PixelFormat format) {
		switch (format) {
				//case Img::PixelFormat::RGB8:
			case Img::PixelFormat::RGBA8:
				return MTLPixelFormatRGBA8Unorm;
			case Img::PixelFormat::BGRA8:
				return MTLPixelFormatBGRA8Unorm;
			case Img::PixelFormat::RGBAF32:
				return MTLPixelFormatRGBA32Float;
			case Img::PixelFormat::RGBAF16:
				return MTLPixelFormatRGBA16Float;
			default:
				assert(false);
		}
		return MTLPixelFormatInvalid;
	}

}
void Renderer::Init(id<MTLDevice> device, MTLPixelFormat pixelFormat) {
	device_ = device;
	commandQueue_ = [device newCommandQueue];
	shaders_ = [[Shaders alloc] initWithDevice:device andPixelFormat:pixelFormat];

	frameBoundarySemaphore_ = dispatch_semaphore_create(ShaderStructures::FrameCount);

	MTLDepthStencilDescriptor *depthStencilDescriptor = [MTLDepthStencilDescriptor new];
	depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLessEqual;	// less AND equal bc. of bg. shader xyww trick, depth write/test should jut be turned off instead
	depthStencilDescriptor.depthWriteEnabled = YES;
	depthStencilState_ = [device newDepthStencilStateWithDescriptor:depthStencilDescriptor];

	{
		// create default sampler state
		MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
		samplerDesc.rAddressMode = MTLSamplerAddressModeRepeat;
		samplerDesc.sAddressMode = MTLSamplerAddressModeRepeat;
		samplerDesc.tAddressMode = MTLSamplerAddressModeRepeat;
		samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
		defaultSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
	}
	{
		// create deferred sampler state
		MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
		samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.minFilter = MTLSamplerMinMagFilterNearest;
		samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
		samplerDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
		deferredSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
	}
	{
		// create mimpapped sampler state
		MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
		samplerDesc.rAddressMode = MTLSamplerAddressModeRepeat;
		samplerDesc.sAddressMode = MTLSamplerAddressModeRepeat;
		samplerDesc.tAddressMode = MTLSamplerAddressModeRepeat;
		samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.mipFilter = MTLSamplerMipFilterLinear;
		mipmapSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
	}

	{
		// create clamp to edge sampler state for brdf lookup
		MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
		samplerDesc.rAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
		clampSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
	}

	struct PosUV {
		packed_float2 pos;
		packed_float2 uv;
	};
	// top: uv.y = 0
	PosUV quad[] = {{{-1., -1.f}, {0.f, 1.f}}, {{-1., 1.f}, {0.f, 0.f}}, {{1., -1.f}, {1.f, 1.f}}, {{1., 1.f}, {1.f, 0.f}}};
	fullscreenTexturedQuad_ = [device_ newBufferWithBytes:quad length:sizeof(quad) options: MTLResourceOptionCPUCacheModeDefault];
	for (int i = 0; i < FrameCount; ++i)
		frame_[i].Init(device, defaultCBFrameAllocSize);

	const glm::vec3 at[] = {/*+x*/{1.f, 0.f, 0.f},/*-x*/{-1.f, 0.f, 0.f},/*+y*/{0.f, 1.f, 0.f},/*-y*/{0.f, -1.f, 0.f},/*+z*/{0.f, 0.f, 1.f},/*-z*/{0.f, 0.f, -1.f}},
	up[] = {{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}};
	const int faceCount = 6;
	cubeViewBufInc_ = AlignTo<int, 256>(sizeof(glm::mat4x4));
	cubeViews_ = [device_ newBufferWithLength: cubeViewBufInc_ * faceCount options: MTLResourceCPUCacheModeWriteCombined];
	auto ptr = (uint8_t*)[cubeViews_ contents];
	for (int i = 0; i < faceCount; ++i) {
		glm::mat4x4 v = glm::lookAtLH({0.f, 0.f, 0.f}, at[i], up[i]);
		memcpy(ptr, &v, sizeof(v));
		ptr += cubeViewBufInc_;
	}
}
BufferIndex Renderer::CreateBuffer(size_t length) {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithLength: length options: MTLResourceCPUCacheModeWriteCombined];
	buffers_.push_back(mtlBuffer);
	return (BufferIndex)buffers_.size() - 1;
}
BufferIndex Renderer::CreateBuffer(const void* data, size_t length) {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithBytes:data length:length options: MTLResourceCPUCacheModeWriteCombined];
	buffers_.push_back(mtlBuffer);
	return (BufferIndex)buffers_.size() - 1;
}

TextureIndex Renderer::CreateTexture(const void* data, uint64_t width, uint32_t height, Img::PixelFormat format, const char* label) {
	const auto fmt = PixelFormatToMTLPixelFormat(format);
	const auto bpp = BytesPerPixel(format);
	MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
																								 width:width
																								height:height
																							 mipmapped:false];

	textureDescriptor.usage = MTLTextureUsageShaderRead;
	id<MTLTexture> texture = [device_ newTextureWithDescriptor:textureDescriptor];
	if (label) [texture setLabel: [NSString stringWithUTF8String:label]];
	textures_.push_back({texture, width, height, bpp , fmt});
	if (data) {
		MTLRegion region = MTLRegionMake2D(0, 0, width, height);
		const NSUInteger bytesPerRow = bpp * width;
		[texture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:bytesPerRow];
	}
	return (TextureIndex)textures_.size() - 1;
}
TextureIndex Renderer::GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint32_t dim, ShaderId shader, bool mip,  const char* label) {
	constexpr MTLPixelFormat format = MTLPixelFormatRGBA16Float;
	constexpr uint8_t bytesPerPixel = 8;
	MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat: format
																									size: dim
																							   mipmapped: mip];

	textureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
	if (mip) {
		textureDescriptor.mipmapLevelCount = std::floor(std::log2(dim)) + 1;
	}
	id<MTLTexture> cubeTexture = [device_ newTextureWithDescriptor:textureDescriptor];
	if (label) [cubeTexture setLabel: [NSString stringWithUTF8String:label]];
	textures_.push_back({cubeTexture, dim, dim, bytesPerPixel, format});

	//	MTLCaptureManager* capManager = [MTLCaptureManager sharedCaptureManager];
	//	[capManager startCaptureWithCommandQueue:commandQueue_];

	id<MTLCommandBuffer> commandBuffer = [commandQueue_ commandBuffer];
	auto& pipeline = [shaders_ selectPipeline: shader];
	// TODO:: do the same with 1 drawcall and mrt
	const int count = 6;
	for (int i = 0, cubeViewsBuffOffset = 0; i < count; ++i, cubeViewsBuffOffset += cubeViewBufInc_) {
		MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
		passDescriptor.colorAttachments[0].texture = cubeTexture;
		passDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
		passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
		passDescriptor.colorAttachments[0].slice = i;
		passDescriptor.colorAttachments[0].level = 0;
		id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor: passDescriptor];
		[encoder setRenderPipelineState: pipeline.pipeline];
		[encoder setCullMode: MTLCullModeNone];
		[encoder setVertexBuffer: buffers_[vb] offset: 0 atIndex: 0];
		[encoder setVertexBuffer: cubeViews_ offset: cubeViewsBuffOffset atIndex: 1];
		[encoder setFragmentTexture: textures_[tex].texture atIndex:0];
		[encoder setFragmentSamplerState: defaultSamplerState_ atIndex:0];

		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[ib] indexBufferOffset: 0 instanceCount: 1 baseVertex: 0 baseInstance: 0];
		[encoder endEncoding];

	}
	if (mip) {
		id<MTLBlitCommandEncoder> commandEncoder = [commandBuffer blitCommandEncoder];
		[commandEncoder generateMipmapsForTexture: cubeTexture];
		[commandEncoder endEncoding];
	}
	[commandBuffer commit];
	[commandBuffer waitUntilCompleted];  // TODO:: wait once for all precomputed stuff

	//	[capManager stopCapture];

	return (TextureIndex)textures_.size() - 1;
}
TextureIndex Renderer::GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const Submesh& submesh, uint32_t dim, ShaderId shader, const char* label) {
	constexpr MTLPixelFormat format = MTLPixelFormatRGBA16Float;
	constexpr uint8_t bytesPerPixel = 8;
	constexpr int mipLevelCount = 5;
	MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat: format
																									size: dim
																							   mipmapped: true];
	textureDescriptor.mipmapLevelCount = mipLevelCount;
	textureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
	id<MTLTexture> cubeTexture = [device_ newTextureWithDescriptor:textureDescriptor];
	if (label) [cubeTexture setLabel: [NSString stringWithUTF8String:label]];
	textures_.push_back({cubeTexture, dim, dim, bytesPerPixel, format});

	//		MTLCaptureManager* capManager = [MTLCaptureManager sharedCaptureManager];
	//		[capManager startCaptureWithCommandQueue:commandQueue_];

	id<MTLCommandBuffer> commandBuffer = [commandQueue_ commandBuffer];
	auto& pipeline = [shaders_ selectPipeline: shader];
	// TODO:: do the same with 1 drawcall and mrt
	constexpr int faceCount = 6;
	using RoughnessBuffType = float;
	const int buffLen = AlignTo<int, 256>(sizeof(RoughnessBuffType));
	id<MTLBuffer> roughnessBuff = [device_ newBufferWithLength: buffLen*mipLevelCount options: MTLResourceCPUCacheModeWriteCombined];
	roughnessBuff.label = @"roughness";
	RoughnessBuffType* pRoughness = (RoughnessBuffType*)[roughnessBuff contents];

	using ResolutionBuffType = float;
	id<MTLBuffer> resolutionBuff = [device_ newBufferWithLength: sizeof(ResolutionBuffType) options: MTLResourceCPUCacheModeWriteCombined];
	resolutionBuff.label = @"resolution";
	ResolutionBuffType* pResolution = (ResolutionBuffType*)[resolutionBuff contents];
	*pResolution = textures_[tex].texture.width;
	for (int mipLevel = 0, offset = 0; mipLevel < mipLevelCount; ++mipLevel, offset += buffLen) {
		*pRoughness = (float)mipLevel / (mipLevelCount - 1);
		pRoughness += buffLen / sizeof(RoughnessBuffType);
		for (int i = 0, cubeViewsBuffOffset = 0; i < faceCount; ++i, cubeViewsBuffOffset += cubeViewBufInc_) {
			MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
			passDescriptor.colorAttachments[0].texture = cubeTexture;
			passDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
			passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
			passDescriptor.colorAttachments[0].slice = i;
			passDescriptor.colorAttachments[0].level = mipLevel;
			id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor: passDescriptor];
			[encoder setRenderPipelineState: pipeline.pipeline];
			[encoder setCullMode: MTLCullModeNone];
			[encoder setVertexBuffer: buffers_[vb] offset: 0 atIndex: 0];
			[encoder setVertexBuffer: cubeViews_ offset: cubeViewsBuffOffset atIndex: 1];
			[encoder setFragmentTexture: textures_[tex].texture atIndex:0];
			[encoder setFragmentSamplerState: mipmapSamplerState_ atIndex:0];
			[encoder setFragmentBuffer: roughnessBuff offset:offset atIndex:0];
			[encoder setFragmentBuffer: resolutionBuff offset:0 atIndex:1];

			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[ib] indexBufferOffset: 0 instanceCount: 1 baseVertex: 0 baseInstance: 0];
			[encoder endEncoding];

		}
	}
	[commandBuffer commit];
	[commandBuffer waitUntilCompleted];  // TODO:: wait once for all precomputed stuff

	//	[capManager stopCapture];

	return (TextureIndex)textures_.size() - 1;
}
TextureIndex Renderer::GenBRDFLUT(uint32_t dim, ShaderId shader, const char* label) {
	constexpr MTLPixelFormat format = MTLPixelFormatRG16Float;
	constexpr uint8_t bytesPerPixel = 8;
	MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: format
																								 width: dim
																								height: dim
																							 mipmapped: false];
	textureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
	id<MTLTexture> tex = [device_ newTextureWithDescriptor:textureDescriptor];
	if (label) [tex setLabel: [NSString stringWithUTF8String:label]];
	textures_.push_back({tex, dim, dim, bytesPerPixel, format});

	id<MTLCommandBuffer> commandBuffer = [commandQueue_ commandBuffer];
	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	passDescriptor.colorAttachments[0].texture = tex;
	passDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	passDescriptor.colorAttachments[0].level = 0;
	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor: passDescriptor];
	[encoder setRenderPipelineState: [shaders_ selectPipeline: shader].pipeline];
	[encoder setCullMode: MTLCullModeNone];
	[encoder setVertexBuffer: fullscreenTexturedQuad_ offset: 0 atIndex: 0];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart: 0 vertexCount: 4 instanceCount: 1 baseInstance: 0];
	[encoder endEncoding];
	[commandBuffer commit];
	[commandBuffer waitUntilCompleted]; // TODO:: wait once for all precomputed stuff
	return (TextureIndex)textures_.size() - 1;
}
void Renderer::MakeColorAttachmentTextures(NSUInteger width, NSUInteger height) {
	if (!colorAttachmentTextures_[0][0] || [colorAttachmentTextures_[0][0] width] != width ||
		[colorAttachmentTextures_[0][0] height] != height) {
		for (int i = 0; i < FrameCount; ++i) {
			for (int j = 0; j < RenderTargetCount; ++j) {
				MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:[shaders_ getColorAttachmentFormats][j]
																								width:width
																							   height:height
																							mipmapped:NO];
				desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
				desc.resourceOptions = MTLResourceStorageModePrivate;
				colorAttachmentTextures_[i][j] = [device_ newTextureWithDescriptor:desc];
				[colorAttachmentTextures_[i][j] setLabel:@"Color Attachment"];
			}
			// debug...
			MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: MTLPixelFormatRGBA32Float
																							width:width
																						   height:height
																						mipmapped:NO];
			desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			desc.resourceOptions = MTLResourceStorageModePrivate;
			deferredDebugColorAttachments_[i] = [device_ newTextureWithDescriptor:desc];
			[deferredDebugColorAttachments_[i]  setLabel:@"Deferred debug color attachment"];
		}
	}
}
void Renderer::MakeDepthTexture(NSUInteger width, NSUInteger height) {
	if (!depthTextures_[0] || [depthTextures_[0] width] != width ||
		[depthTextures_[0] height] != height) {
		MTLPixelFormat pf = MTLPixelFormatDepth32Float;
		MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pf
																						width:width
																					   height:height
																					mipmapped:NO];
		desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		desc.resourceOptions = MTLResourceStorageModePrivate;

		MTLTextureDescriptor *descHalfRes = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
																							   width:width>>1
																							  height:height>>1
																						   mipmapped:NO];
		descHalfRes.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		descHalfRes.resourceOptions = MTLResourceStorageModePrivate;
		for (int i = 0; i < FrameCount; ++i) {
			id<MTLTexture> texture = [device_ newTextureWithDescriptor:desc];
			[texture setLabel: [NSString stringWithFormat: @"Depth Texture %d", i]];
			depthTextures_[i] = texture;

			texture = [device_ newTextureWithDescriptor:descHalfRes];
			[texture setLabel: [NSString stringWithFormat: @"HalfResDepth Texture %d", i]];
			halfResDepthTextures_[i] = texture;
		}
	}
}
void Renderer::BeginRender() {
	id<MTLTexture> surface = drawable_.texture;
	dispatch_semaphore_wait(frameBoundarySemaphore_, DISPATCH_TIME_FOREVER);
	MakeDepthTexture([surface width], [surface height]);
	MakeColorAttachmentTextures([surface width], [surface height]);
	// TODO:: storing the surface currently redundant
	commandBuffers_.reserve([shaders_ getPipelineCount]);
	for (size_t i = 0; i < [shaders_ getPipelineCount]; ++i)
		commandBuffers_.push_back([commandQueue_ commandBuffer]);
	encoders_.resize(commandBuffers_.size());
	frame_[currentFrameIndex_].Reset();
}
void Renderer::BeginPrePass() {}
void Renderer::EndPrePass() {}
void Renderer::StartGeometryPass() {
	//	MTLCaptureManager* capManager = [MTLCaptureManager sharedCaptureManager];
	//	if (![capManager isCapturing]) [capManager startCaptureWithCommandQueue:commandQueue_];

	MTLRenderPassDescriptor *firstPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	for (int i = 0; i < RenderTargetCount; ++i) {
		firstPassDescriptor.colorAttachments[i].texture = colorAttachmentTextures_[currentFrameIndex_][i];
		firstPassDescriptor.colorAttachments[i].loadAction = MTLLoadActionClear;
		firstPassDescriptor.colorAttachments[i].storeAction = MTLStoreActionStore;
		firstPassDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(0.f, 0., 0., 0.f);
	}
	firstPassDescriptor.depthAttachment.texture = depthTextures_[currentFrameIndex_];
	firstPassDescriptor.depthAttachment.clearDepth = 1.f;
	firstPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
	firstPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;

	MTLRenderPassDescriptor *followingPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	for (int i = 0; i < RenderTargetCount; ++i) {
		followingPassDescriptor.colorAttachments[i].texture = colorAttachmentTextures_[currentFrameIndex_][i];
		followingPassDescriptor.colorAttachments[i].loadAction = MTLLoadActionDontCare;
		followingPassDescriptor.colorAttachments[i].storeAction = MTLStoreActionStore;
	}
	followingPassDescriptor.depthAttachment.texture = depthTextures_[currentFrameIndex_];
	followingPassDescriptor.depthAttachment.clearDepth = 1.f;
	followingPassDescriptor.depthAttachment.loadAction = MTLLoadActionDontCare;
	followingPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;

	bool first = true;
	for (size_t i = 0; i < [shaders_ getPipelineCount]; ++i) {
		auto& pipeline = [shaders_ selectPipeline: i];
		if (pipeline.renderPass == RenderPass::Geometry) {
			MTLRenderPassDescriptor * passDesc = (first) ? firstPassDescriptor : followingPassDescriptor;
			auto encoder = [commandBuffers_[i] renderCommandEncoderWithDescriptor: passDesc];
			encoders_[i] = encoder;
			[encoder setRenderPipelineState: pipeline.pipeline];
			[encoder setDepthStencilState: depthStencilState_];
			[encoder setCullMode: MTLCullModeBack];
			first = false;
		}
	};
}
void Renderer::StartForwardPass() {
	id<MTLTexture> surface = drawable_.texture;
	MTLRenderPassDescriptor *firstPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	firstPassDescriptor.colorAttachments[0].texture = surface;
	firstPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
	firstPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	firstPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.f, 0.f, 0.f, 0.f);
	//	firstPassDescriptor.depthAttachment.texture = depthTextures_[currentFrameIndex_];
	//	firstPassDescriptor.depthAttachment.clearDepth = 1.f;
	//	firstPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
	//	firstPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;

	MTLRenderPassDescriptor *followingPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	followingPassDescriptor.colorAttachments[0].texture = surface;
	followingPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	followingPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	//	followingPassDescriptor.depthAttachment.texture = depthTextures_[currentFrameIndex_];
	//	followingPassDescriptor.depthAttachment.clearDepth = 1.f;
	//	followingPassDescriptor.depthAttachment.loadAction = MTLLoadActionDontCare;
	//	followingPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;

	bool first = true;
	for (size_t i = 0; i < [shaders_ getPipelineCount]; ++i) {
		auto& pipeline = [shaders_ selectPipeline: i];
		if (pipeline.renderPass == RenderPass::Forward) {
			MTLRenderPassDescriptor * passDesc = (first) ? firstPassDescriptor : followingPassDescriptor;
			auto encoder = [commandBuffers_[i] renderCommandEncoderWithDescriptor: passDesc];
			encoders_[i] = encoder;
			[encoder setRenderPipelineState: pipeline.pipeline];
			//[encoder setDepthStencilState: depthStencilState_];
			[encoder setCullMode: MTLCullModeNone];
			first = false;
		}
	};
}
void Renderer::BeginUploadResources() {
	// TODO::
}
void Renderer::EndUploadResources() {
	// TODO::
}


void Renderer::Downsample(id<MTLCommandBuffer> _Nonnull commandBuffer,
						 id<MTLRenderPipelineState> _Nonnull pipelineState,
						 id<MTLTexture> _Nonnull srcTex,
						 id<MTLTexture> _Nonnull dstTex) {
	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	passDescriptor.colorAttachments[0].texture = dstTex;
	passDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor: passDescriptor];

	[encoder setCullMode: MTLCullModeBack];
	[encoder setRenderPipelineState: pipelineState];
	[encoder setVertexBuffer: fullscreenTexturedQuad_ offset: 0 atIndex: 0];
	[encoder setFragmentTexture: srcTex atIndex: 0];

	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	struct {
		unsigned int width, height;
	}buf;
	auto cb = frame.Alloc(sizeof(buf));
	buf.width = (unsigned int)srcTex.width; buf.height = (unsigned int)srcTex.height;
	memcpy(cb.address, &buf, sizeof(buf));
	[encoder setFragmentBuffer:cb.buffer  offset: cb.offset atIndex: 0];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart: 0 vertexCount: 4 instanceCount: 1 baseInstance: 0];
	[encoder endEncoding];
}
void Renderer::DoLightingPass(const ShaderStructures::DeferredCmd& cmd) {
	deferredCommandBuffer_ = [commandQueue_ commandBuffer];

	Downsample(deferredCommandBuffer_, [shaders_ selectPipeline: ShaderStructures::Downsample].pipeline,
				depthTextures_[currentFrameIndex_],
				halfResDepthTextures_[currentFrameIndex_]);

	MTLRenderPassDescriptor *deferredPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	deferredPassDescriptor.colorAttachments[0].texture = drawable_.texture;
	deferredPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	deferredPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	//debug...
	deferredPassDescriptor.colorAttachments[1].texture = deferredDebugColorAttachments_[currentFrameIndex_];
	deferredPassDescriptor.colorAttachments[1].loadAction = MTLLoadActionDontCare;
	deferredPassDescriptor.colorAttachments[1].storeAction = MTLStoreActionStore;

	id<MTLRenderCommandEncoder> deferredEncoder = [deferredCommandBuffer_ renderCommandEncoderWithDescriptor: deferredPassDescriptor];
	[deferredEncoder setCullMode: MTLCullModeBack];
	[deferredEncoder setRenderPipelineState: [shaders_ selectPipeline: DeferredPBR].pipeline];
	[deferredEncoder setVertexBuffer: fullscreenTexturedQuad_ offset: 0 atIndex: 0];
	NSUInteger fsTexIndex = 0;
	for (; fsTexIndex < RenderTargetCount; ++fsTexIndex) {
		[deferredEncoder setFragmentTexture: colorAttachmentTextures_[currentFrameIndex_][fsTexIndex] atIndex:fsTexIndex];
	}
	[deferredEncoder setFragmentTexture: depthTextures_[currentFrameIndex_] atIndex:fsTexIndex++];
	assert(cmd.irradiance != InvalidTexture);
	[deferredEncoder setFragmentTexture: textures_[cmd.irradiance].texture atIndex:fsTexIndex++];
	assert(cmd.prefilteredEnvMap != InvalidTexture);
	[deferredEncoder setFragmentTexture: textures_[cmd.prefilteredEnvMap].texture atIndex:fsTexIndex++];
	assert(cmd.BRDFLUT != InvalidTexture);
	[deferredEncoder setFragmentTexture: textures_[cmd.BRDFLUT].texture atIndex:fsTexIndex++];
	[deferredEncoder setFragmentTexture: halfResDepthTextures_[currentFrameIndex_] atIndex:fsTexIndex++];
	[deferredEncoder setFragmentTexture: textures_[cmd.random].texture atIndex:fsTexIndex++];
	[deferredEncoder setFragmentSamplerState:deferredSamplerState_ atIndex:0];
	[deferredEncoder setFragmentSamplerState:defaultSamplerState_ atIndex:1];
	[deferredEncoder setFragmentSamplerState:mipmapSamplerState_ atIndex:2];
	[deferredEncoder setFragmentSamplerState:clampSamplerState_ atIndex:3];

	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	int fsIndex = 0;
	{
		auto cb = frame.Alloc(sizeof(cmd.scene));
		memcpy(cb.address, &cmd.scene, sizeof(cmd.scene));
		[deferredEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsIndex++];
	}
	{
		auto cb = frame.Alloc(sizeof(cmd.ao));
		memcpy(cb.address, &cmd.ao, sizeof(cmd.ao));
		[deferredEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsIndex++];
	}

	[deferredEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart: 0 vertexCount: 4 instanceCount: 1 baseInstance: 0];
	[deferredEncoder endEncoding];
}
void Renderer::Render() {
	//	for (auto commandEncoder : encoders_) {
	//		[commandEncoder endEncoding];
	//	}
	//
	//	assert(!commandBuffers_.empty());
	//	[commandBuffers_.back() presentDrawable:drawable];
	//
	//	__weak dispatch_semaphore_t semaphore = frameBoundarySemaphore_;
	//	[commandBuffers_.back() addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
	//		// GPU work is complete
	//		// Signal the semaphore to start the CPU work
	//		dispatch_semaphore_signal(semaphore);
	//	}];
	//	for (auto commandBuffer : commandBuffers_) {
	//		[commandBuffer commit];
	//	}
	//
	//	// TODO: is this necessary?
	//	encoders_.clear();
	//	// TODO: is this necessary?
	//	commandBuffers_.clear();
	//	currentFrameIndex_ = (currentFrameIndex_ + 1) % ShaderStructures::FrameCount;
	//	//MTLCaptureManager* capManager = [MTLCaptureManager sharedCaptureManager];
	//	//[capManager stopCapture];

	for (auto commandEncoder : encoders_) {
		[commandEncoder endEncoding];
	}
	// @@@ Test fwd rendering pass
	//	assert(!commandBuffers_.empty());
	//	[commandBuffers_.back() presentDrawable:drawable];
	//
	//	__weak dispatch_semaphore_t semaphore = frameBoundarySemaphore_;
	//	[commandBuffers_.back() addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
	//		// GPU work is complete
	//		// Signal the semaphore to start the CPU work
	//		dispatch_semaphore_signal(semaphore);
	//	}];
	// @@@

	// resize depth
	for (auto commandBuffer : commandBuffers_) {
		[commandBuffer commit];
	}
	[deferredCommandBuffer_ presentDrawable: drawable_];
	__weak dispatch_semaphore_t semaphore = frameBoundarySemaphore_;
	[deferredCommandBuffer_ addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
		// GPU work is complete
		// Signal the semaphore to start the CPU work
		dispatch_semaphore_signal(semaphore);
	}];
	[deferredCommandBuffer_ commit];
	// TODO: is this necessary?
	encoders_.clear();
	// TODO: is this necessary?
	commandBuffers_.clear();
	currentFrameIndex_ = (currentFrameIndex_ + 1) % ShaderStructures::FrameCount;
	//MTLCaptureManager* capManager = [MTLCaptureManager sharedCaptureManager];
	//[capManager stopCapture];
}

void Renderer::Submit(const ShaderStructures::BgCmd& cmd) {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[ShaderStructures::Bg];

	NSUInteger vsAttribIndex = 0;
	[commandEncoder setVertexBuffer: buffers_[cmd.vb] offset: cmd.submesh.vbByteOffset atIndex: vsAttribIndex++];

	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	{
		auto cb = frame.Alloc(sizeof(float4x4));
		memcpy(cb.address, &cmd.vp, sizeof(float4x4));
		[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
	}
	{
		auto& texture = textures_[cmd.cubeEnv];
		[commandEncoder setFragmentTexture: texture.texture atIndex:0];
		[commandEncoder setFragmentSamplerState:defaultSamplerState_ atIndex:0];
	}
	[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib] indexBufferOffset: cmd.submesh.ibByteOffset instanceCount: 1 baseVertex: 0 baseInstance: 0];
}

void Renderer::Submit(const ShaderStructures::ModoDrawCmd& cmd) {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[cmd.shader];

	NSUInteger vsAttribIndex = 0, fsAttribIndex = 0, textureIndex = 0;
	[commandEncoder setVertexBuffer: buffers_[cmd.vb] offset: cmd.submesh.vertexByteOffset atIndex: vsAttribIndex++];

	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	switch (cmd.shader) {
		case ShaderStructures::Pos: {
			{
				auto cb = frame.Alloc(sizeof(Object));
				((Object*)cb.address)->m = cmd.o.m;
				((Object*)cb.address)->mvp = cmd.o.mvp;
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			{
				auto cb = frame.Alloc(sizeof(Material));
				((Material*)cb.address)->diffuse = cmd.material.diffuse;
				((Material*)cb.address)->metallic_roughness = cmd.material.metallic_roughness;
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			break;
		}
		case ShaderStructures::ModoDNMR: {
			{
				auto cb = frame.Alloc(sizeof(Object));
				((Object*)cb.address)->m = cmd.o.m;
				((Object*)cb.address)->mvp = cmd.o.mvp;
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			for (int i = 0; i < sizeof(cmd.submesh.textures) / sizeof(cmd.submesh.textures[0]); ++i) {
				if (!(cmd.submesh.textureMask & (1 << i))) continue;
				auto& texture = textures_[cmd.submesh.textures[i].id];
				[commandEncoder setFragmentTexture: texture.texture atIndex: textureIndex++];
				[commandEncoder setFragmentSamplerState:defaultSamplerState_ atIndex:0];
			}
			break;
		}
		case ShaderStructures::ModoDN: {
			{
				auto cb = frame.Alloc(sizeof(Object));
				((Object*)cb.address)->m = cmd.o.m;
				((Object*)cb.address)->mvp = cmd.o.mvp;
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			{
				auto cb = frame.Alloc(sizeof(Material));
				auto* material = (Material*)cb.address;
				material->diffuse = cmd.material.diffuse;
				material->metallic_roughness = cmd.material.metallic_roughness;
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			for (int i = 0; i < sizeof(cmd.submesh.textures) / sizeof(cmd.submesh.textures[0]); ++i) {
				if (!(cmd.submesh.textureMask & (1 << i))) continue;
				auto& texture = textures_[cmd.submesh.textures[i].id];
				[commandEncoder setFragmentTexture: texture.texture atIndex: textureIndex++];
				[commandEncoder setFragmentSamplerState:defaultSamplerState_ atIndex:0];
			}
			break;
		}
		default: assert(false);
	}
	[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib] indexBufferOffset: cmd.submesh.indexByteOffset instanceCount: 1 baseVertex: 0 baseInstance: 0];
}

void Renderer::Submit(const ShaderStructures::DrawCmd& cmd) {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[cmd.shader];

	NSUInteger vsAttribIndex = 0, fsAttribIndex = 0, textureIndex = 0;
	[commandEncoder setVertexBuffer: buffers_[cmd.vb] offset: cmd.submesh.vbByteOffset atIndex: vsAttribIndex++];

	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	switch (cmd.shader) {
		case ShaderStructures::Debug: {
			{
				auto cb = frame.Alloc(sizeof(float4x4));
				memcpy(cb.address, &cmd.mvp, sizeof(float4x4));
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			{
				auto cb = frame.Alloc(sizeof(float4));
				memcpy(cb.address, &cmd.material.diffuse, sizeof(float3));
				((float*)cb.address)[3] = 1.f;
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			break;
		}
		case ShaderStructures::Pos: {
			{
				auto cb = frame.Alloc(sizeof(Object));
				((Object*)cb.address)->m = cmd.m;
				((Object*)cb.address)->mvp = cmd.mvp;
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			{
				auto cb = frame.Alloc(sizeof(Material));
				((Material*)cb.address)->diffuse = cmd.material.diffuse;
				((Material*)cb.address)->metallic_roughness = cmd.material.metallic_roughness;
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			break;
		}
		case ShaderStructures::Tex: {
			{
				auto cb = frame.Alloc(sizeof(Object));
				((Object*)cb.address)->m = cmd.m;
				((Object*)cb.address)->mvp = cmd.mvp;
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			{
				auto& texture = textures_[cmd.submesh.texAlbedo];
				[commandEncoder setFragmentTexture: texture.texture atIndex:textureIndex++];
				[commandEncoder setFragmentSamplerState:defaultSamplerState_ atIndex:0];
			}
			{
				auto cb = frame.Alloc(sizeof(Material));
				((Material*)cb.address)->diffuse = cmd.material.diffuse;
				((Material*)cb.address)->metallic_roughness = cmd.material.metallic_roughness;
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			break;
		}
		default: assert(false);
	}
	[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib] indexBufferOffset: cmd.submesh.ibByteOffset instanceCount: 1 baseVertex: 0 baseInstance: 0];
}
uint32_t Renderer::GetCurrenFrameIndex() const { return currentFrameIndex_; }
void Renderer::SetDrawable(id<CAMetalDrawable> _Nonnull drawable) { drawable_ = drawable; }
Dim Renderer::GetDimensions(TextureIndex index) {
	assert(index <= textures_.size());
	return {(uint32_t)textures_[index].width, textures_[index].height};
}
