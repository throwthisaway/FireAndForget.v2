#import "Renderer.h"
//#include <simd/vector_types.h>
#include "cpp/Assets.hpp"
#include "cpp/BufferUtils.h"
#include <glm/gtc/matrix_transform.hpp>
//#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#include "UI.h"
#include "cpp/DebugUI.h"
#include "cpp/GenSSAOKernel.hpp"

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
	DEBUGUI(ui::Init(device));
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
		linearWrapSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
	}
	{
		// create deferred sampler state
		MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
		samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.minFilter = MTLSamplerMinMagFilterNearest;
		samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
		samplerDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
		nearestClampSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
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
		linearWrapMipSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
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
		linearClampSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
	}

	struct PosUV {
		packed_float2 pos;
		packed_float2 uv;
	};

	for (int i = 0; i < FrameCount; ++i)
		frame_[i].Init(device, defaultCBFrameAllocSize);

	const glm::vec3 at[] = {/*+x*/{1.f, 0.f, 0.f},/*-x*/{-1.f, 0.f, 0.f},/*+y*/{0.f, 1.f, 0.f},/*-y*/{0.f, -1.f, 0.f},/*+z*/{0.f, 0.f, 1.f},/*-z*/{0.f, 0.f, -1.f}},
	up[] = {{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}};
	const int faceCount = 6;
	cubeViewBufInc_ = AlignTo<256>(sizeof(glm::mat4x4));
	cubeViews_ = [device_ newBufferWithLength: cubeViewBufInc_ * faceCount options: MTLResourceCPUCacheModeWriteCombined];
	auto ptr = (uint8_t*)[cubeViews_ contents];
	for (int i = 0; i < faceCount; ++i) {
		glm::mat4x4 v = glm::lookAtLH({0.f, 0.f, 0.f}, at[i], up[i]);
		memcpy(ptr, &v, sizeof(v));
		ptr += cubeViewBufInc_;
	}

	auto kernel = GenSSAOKernel();
	auto size = kernel.size() * sizeof(kernel.front());
	ssaoKernelBufferIndex_ = CreateBuffer(kernel.data(), size);
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
TextureIndex Renderer::GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const ModoMeshLoader::Submesh& submesh, uint32_t dim, ShaderId shader, bool mip,  const char* label) {
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
		[encoder setFragmentSamplerState: linearWrapSamplerState_ atIndex:0];

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
TextureIndex Renderer::GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const ModoMeshLoader::Submesh& submesh, uint32_t dim, ShaderId shader, const char* label) {
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
	const int buffLen = AlignTo<256>(sizeof(RoughnessBuffType));
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
			[encoder setFragmentSamplerState: linearWrapMipSamplerState_ atIndex:0];
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
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart: 0 vertexCount: 4 instanceCount: 1 baseInstance: 0];
	[encoder endEncoding];
	[commandBuffer commit];
	[commandBuffer waitUntilCompleted]; // TODO:: wait once for all precomputed stuff
	return (TextureIndex)textures_.size() - 1;
}
void Renderer::MakeColorAttachmentTextures(NSUInteger width, NSUInteger height) {
	if (!colorAttachmentTextures_[0] || [colorAttachmentTextures_[0] width] != width ||
		[colorAttachmentTextures_[0] height] != height) {
		for (int j = 0; j < RenderTargetCount; ++j) {
			MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:[shaders_ getColorAttachmentFormats][j]
																							width:width
																						   height:height
																						mipmapped:NO];
			desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			desc.resourceOptions = MTLResourceStorageModePrivate;
			colorAttachmentTextures_[j] = [device_ newTextureWithDescriptor:desc];
			[colorAttachmentTextures_[j] setLabel:@"Color Attachment"];
		}
		// debug...
		for (int i = 0; i < FrameCount; ++i) {
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
	if (!depthTexture_ || [depthTexture_ width] != width ||
		[depthTexture_ height] != height) {
		MTLPixelFormat pf = MTLPixelFormatDepth32Float;
		MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pf
																						width:width
																					   height:height
																					mipmapped:NO];
		desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		desc.resourceOptions = MTLResourceStorageModePrivate;

		id<MTLTexture> texture = [device_ newTextureWithDescriptor:desc];
		[texture setLabel: @"Depth Texture"];
		depthTexture_ = texture;

		MTLTextureDescriptor *descHalfRes = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
																							   width:width>>1
																							  height:height>>1
																						   mipmapped:NO];
		descHalfRes.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		descHalfRes.resourceOptions = MTLResourceStorageModePrivate;

		texture = [device_ newTextureWithDescriptor:descHalfRes];
		[texture setLabel: @"HalfResDepth Texture"];
		halfResDepthTexture_ = texture;

		texture = [device_ newTextureWithDescriptor:descHalfRes];
		[texture setLabel: @"SSAO Texture"];
		ssaoTexture_ = texture;

		texture = [device_ newTextureWithDescriptor:descHalfRes];
		[texture setLabel: @"SSAO Texture"];
		ssaoBlurTexture_ = texture;
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
		firstPassDescriptor.colorAttachments[i].texture = colorAttachmentTextures_[i];
		firstPassDescriptor.colorAttachments[i].loadAction = MTLLoadActionClear;
		firstPassDescriptor.colorAttachments[i].storeAction = MTLStoreActionStore;
		firstPassDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(0.f, 0., 0., 0.f);
	}
	firstPassDescriptor.depthAttachment.texture = depthTexture_;
	firstPassDescriptor.depthAttachment.clearDepth = 1.f;
	firstPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
	firstPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;

	MTLRenderPassDescriptor *followingPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	for (int i = 0; i < RenderTargetCount; ++i) {
		followingPassDescriptor.colorAttachments[i].texture = colorAttachmentTextures_[i];
		followingPassDescriptor.colorAttachments[i].loadAction = MTLLoadActionDontCare;
		followingPassDescriptor.colorAttachments[i].storeAction = MTLStoreActionStore;
	}
	followingPassDescriptor.depthAttachment.texture = depthTexture_;
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
	deferredCommandBuffer_ = [commandQueue_ commandBuffer];
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
void Renderer::SSAOBlurPass() {
	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	passDescriptor.colorAttachments[0].texture = ssaoBlurTexture_;
	passDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLRenderCommandEncoder> encoder = [deferredCommandBuffer_ renderCommandEncoderWithDescriptor: passDescriptor];
	[encoder setCullMode: MTLCullModeBack];
	[encoder setRenderPipelineState: [shaders_ selectPipeline: Blur4x4R32].pipeline];

	[encoder setFragmentTexture: ssaoTexture_ atIndex:0];
	[encoder setFragmentSamplerState: linearClampSamplerState_ atIndex:0];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart: 0 vertexCount: 4 instanceCount: 1 baseInstance: 0];
	[encoder endEncoding];
}
void Renderer::SSAOPass(const ShaderStructures::SSAOCmd& cmd) {
	Downsample(deferredCommandBuffer_, [shaders_ selectPipeline: ShaderStructures::Downsample].pipeline,
			depthTexture_,
			halfResDepthTexture_);
	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	passDescriptor.colorAttachments[0].texture = ssaoTexture_;
	passDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLRenderCommandEncoder> encoder = [deferredCommandBuffer_ renderCommandEncoderWithDescriptor: passDescriptor];
	[encoder setCullMode: MTLCullModeBack];
	[encoder setRenderPipelineState: [shaders_ selectPipeline: SSAOShader].pipeline];

	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	{
		auto cb = frame.Alloc(sizeof(cmd.ip));
		memcpy(cb.address, &cmd.ip, sizeof(cmd.ip));
		[encoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: 0];
	}
	int fsIndex = 0;
	{
		auto cb = frame.Alloc(sizeof(cmd.scene));
		memcpy(cb.address, &cmd.scene, sizeof(cmd.scene));
		[encoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsIndex++];
	}
	{
		auto cb = frame.Alloc(sizeof(cmd.ao));
		memcpy(cb.address, &cmd.ao, sizeof(cmd.ao));
		[encoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsIndex++];
	}
	[encoder setFragmentBuffer: buffers_[ssaoKernelBufferIndex_] offset: 0 atIndex: fsIndex++];

	NSUInteger fsTexIndex = 0;
	// half-res depth
	[encoder setFragmentTexture: halfResDepthTexture_ atIndex:fsTexIndex++];
	// normalVS
	[encoder setFragmentTexture: colorAttachmentTextures_[2] atIndex:fsTexIndex++];
	// random
	[encoder setFragmentTexture: textures_[cmd.random].texture atIndex:fsTexIndex++];

	[encoder setFragmentSamplerState: linearClampSamplerState_ atIndex:0];
	[encoder setFragmentSamplerState: linearWrapSamplerState_ atIndex:1];

	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart: 0 vertexCount: 4 instanceCount: 1 baseInstance: 0];
	[encoder endEncoding];
	SSAOBlurPass();
}
void Renderer::DoLightingPass(const ShaderStructures::DeferredCmd& cmd) {

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
	NSUInteger fsTexIndex = 0;
	// albedo
	[deferredEncoder setFragmentTexture: colorAttachmentTextures_[0] atIndex:fsTexIndex++];
	// normalWS
	[deferredEncoder setFragmentTexture: colorAttachmentTextures_[1] atIndex:fsTexIndex++];
	// material
	[deferredEncoder setFragmentTexture: colorAttachmentTextures_[3] atIndex:fsTexIndex++];
	// debug
	[deferredEncoder setFragmentTexture: colorAttachmentTextures_[4] atIndex:fsTexIndex++];

	[deferredEncoder setFragmentTexture: depthTexture_ atIndex:fsTexIndex++];
	assert(cmd.irradiance != InvalidTexture);
	[deferredEncoder setFragmentTexture: textures_[cmd.irradiance].texture atIndex:fsTexIndex++];
	assert(cmd.prefilteredEnvMap != InvalidTexture);
	[deferredEncoder setFragmentTexture: textures_[cmd.prefilteredEnvMap].texture atIndex:fsTexIndex++];
	assert(cmd.BRDFLUT != InvalidTexture);
	[deferredEncoder setFragmentTexture: textures_[cmd.BRDFLUT].texture atIndex:fsTexIndex++];

	[deferredEncoder setFragmentTexture: ssaoBlurTexture_ atIndex:fsTexIndex++];
	[deferredEncoder setFragmentSamplerState:nearestClampSamplerState_ atIndex:0];
	[deferredEncoder setFragmentSamplerState:linearWrapSamplerState_ atIndex:1];
	[deferredEncoder setFragmentSamplerState:linearWrapMipSamplerState_ atIndex:2];
	[deferredEncoder setFragmentSamplerState:linearClampSamplerState_ atIndex:3];

	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	int fsIndex = 0;
	{
		auto cb = frame.Alloc(sizeof(cmd.scene));
		memcpy(cb.address, &cmd.scene, sizeof(cmd.scene));
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
	id<MTLCommandBuffer> commandBuffer;
#ifdef DEBUG_UI
	commandBuffer = ui::Render([drawable_ texture], [commandQueue_ commandBuffer]);
	[deferredCommandBuffer_ commit];
#else
	commandBuffer = deferredCommandBuffer_;
#endif
	[commandBuffer presentDrawable: drawable_];
	__weak dispatch_semaphore_t semaphore = frameBoundarySemaphore_;
	[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
		// GPU work is complete
		// Signal the semaphore to start the CPU work
		dispatch_semaphore_signal(semaphore);
	}];
	[commandBuffer commit];
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
	[commandEncoder setVertexBuffer: buffers_[cmd.vb] offset: cmd.submesh.vertexByteOffset atIndex: vsAttribIndex++];

	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	{
		auto cb = frame.Alloc(sizeof(float4x4));
		memcpy(cb.address, &cmd.vp, sizeof(float4x4));
		[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
	}
	{
		auto& texture = textures_[cmd.cubeEnv];
		[commandEncoder setFragmentTexture: texture.texture atIndex:0];
		[commandEncoder setFragmentSamplerState:linearWrapSamplerState_ atIndex:0];
	}
	[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib] indexBufferOffset: cmd.submesh.indexByteOffset instanceCount: 1 baseVertex: 0 baseInstance: 0];
}

template<typename T>
CBFrameAlloc::Entry Renderer::Bind(const T& data) {
	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	auto cb = frame.Alloc(sizeof(T));
	memcpy(cb.address, &data, sizeof(T));
	return cb;
}
void Renderer::Submit(const ShaderStructures::ModoDrawCmd& cmd) {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[cmd.shader];

	NSUInteger vsAttribIndex = 0, fsAttribIndex = 0, textureIndex = 0;
	[commandEncoder setVertexBuffer: buffers_[cmd.vb] offset: cmd.submesh.vertexByteOffset atIndex: vsAttribIndex++];

	CBFrameAlloc& frame = frame_[currentFrameIndex_];
	switch (cmd.shader) {
		case ShaderStructures::Pos: {
			{
				auto cb = Bind(cmd.o);
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			{
				auto cb = Bind(cmd.material);
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			break;
		}
		case ShaderStructures::ModoDNMR: {
			{
				auto cb = Bind(cmd.o);
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			for (int i = 0; i < sizeof(cmd.submesh.textures) / sizeof(cmd.submesh.textures[0]); ++i) {
				if (!(cmd.submesh.textureMask & (1 << i))) continue;
				auto& texture = textures_[cmd.submesh.textures[i].id];
				[commandEncoder setFragmentTexture: texture.texture atIndex: textureIndex++];
				[commandEncoder setFragmentSamplerState:linearWrapSamplerState_ atIndex:0];
			}
			break;
		}
		case ShaderStructures::ModoDN: {
			{
				auto cb = Bind(cmd.o);
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			{
				auto cb = Bind(cmd.material);
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			for (int i = 0; i < sizeof(cmd.submesh.textures) / sizeof(cmd.submesh.textures[0]); ++i) {
				if (!(cmd.submesh.textureMask & (1 << i))) continue;
				auto& texture = textures_[cmd.submesh.textures[i].id];
				[commandEncoder setFragmentTexture: texture.texture atIndex: textureIndex++];
				[commandEncoder setFragmentSamplerState:linearWrapSamplerState_ atIndex:0];
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
				[commandEncoder setFragmentSamplerState:linearWrapSamplerState_ atIndex:0];
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
void Renderer::Update(double frame, double total) {
	DEBUGUI(ui::Update(frame, total));
};
void Renderer::OnResize(int width, int height) {
	DEBUGUI(ui::OnResize(width, height));
}
