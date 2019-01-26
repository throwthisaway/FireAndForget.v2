#import "Renderer.h"
#import "Shaders.h"
#import "cpp/RendererWrapper.h"
#include <simd/vector_types.h>
#include "cpp/Assets.hpp"
#include "cpp/BufferUtils.h"
#include "CBFrameAlloc.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace ShaderStructures;

void RendererWrapper::Init(void* renderer) {
	this->renderer = renderer;
}
BufferIndex RendererWrapper::CreateBuffer(const void* data, size_t length) {
	return [(__bridge Renderer*)renderer createBuffer:data withLength:length];
}

template<>
void RendererWrapper::Submit(const ShaderStructures::BgCmd& cmd) {
	[(__bridge Renderer*)renderer submitBgCmd: cmd];
}
template<>
void RendererWrapper::Submit(const ShaderStructures::DebugCmd& cmd) {
	[(__bridge Renderer*)renderer submitDebugCmd: cmd];
}
template<>
void RendererWrapper::Submit(const ShaderStructures::PosCmd& cmd) {
	[(__bridge Renderer*)renderer submitPosCmd: cmd];
}
template<>
void RendererWrapper::Submit(const ShaderStructures::TexCmd& cmd) {
	[(__bridge Renderer*)renderer submitTexCmd: cmd];
}
template<>
void RendererWrapper::Submit(const ShaderStructures::DrawCmd& cmd) {
	[(__bridge Renderer*)renderer submitDrawCmd: cmd];
}
void RendererWrapper::SetDeferredBuffers(const ShaderStructures::DeferredBuffers& deferredBuffers) {
	[(__bridge Renderer*)renderer setDeferredBuffers: deferredBuffers];
}
namespace {
	MTLPixelFormat PixelFormatToMTLPixelFormat(Img::PixelFormat format) {
		switch (format) {
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
TextureIndex RendererWrapper::CreateTexture(const void* data, uint64_t width, uint32_t height, Img::PixelFormat format, const char* label) {
	return [(__bridge Renderer*)renderer createTexture: data withWidth: width withHeight: height withBytesPerPixel: Img::BytesPerPixel(format) withPixelFormat: PixelFormatToMTLPixelFormat(format) withLabel: label ? [NSString stringWithUTF8String:label] : nullptr];
}
TextureIndex RendererWrapper::GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const assets::Submesh& submesh, uint64_t dim, ShaderId shader, const char* label) {
	return [(__bridge Renderer*)renderer genCubeMap: tex withVB: vb withIB: ib withSubmesh: submesh withDimension: dim withShader: shader withLabel: label ? [NSString stringWithUTF8String:label] : nullptr];
}
void RendererWrapper::BeginRender() {
	//[(__bridge Renderer*)renderer beginRender];
}
void RendererWrapper::StartDeferredPass() {
	[(__bridge Renderer*)renderer startDeferredPass];
}
void RendererWrapper::StartForwardPass() {
	[(__bridge Renderer*)renderer startForwardPass];
}
void RendererWrapper::BeginUploadResources() {
	// TODO::
}
void RendererWrapper::EndUploadResources() {
	// TODO::
}
uint32_t RendererWrapper::GetCurrenFrameIndex() {
	return [(__bridge Renderer*)renderer getCurrentFrameIndex];
}
ShaderResourceIndex RendererWrapper::CreateShaderResource(uint32_t length, uint16_t count) {
	return [(__bridge Renderer*)renderer createShaderResource: length withCount: count];
}

void RendererWrapper::UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size) {
	return [(__bridge Renderer*)renderer updateShaderResource: shaderResourceIndex withData: data andSize: size];
}

struct Texture {
	id<MTLTexture> texture;
	uint64_t width;
	uint32_t height;
	uint8_t bytesPerPixel;
	MTLPixelFormat format;

};
namespace {
	static constexpr size_t defaultCBFrameAllocSize = 65536;
};
@implementation Renderer {
	id<MTLDevice> device_;
	id<MTLTexture> surface_;
	id<MTLCommandQueue> commandQueue_;
	id<MTLTexture> depthTextures_[FrameCount];
	id<MTLTexture> colorAttachmentTextures_[FrameCount][RenderTargetCount];
	std::vector<id<MTLBuffer>> buffers_;
	std::vector<Texture> textures_;
	std::vector<id<MTLRenderCommandEncoder>> encoders_;
	std::vector<id<MTLCommandBuffer>> commandBuffers_;
	Shaders* shaders_;
	id<MTLDepthStencilState> depthStencilState_;

	id<MTLSamplerState> defaultSamplerState_, deferredSamplerState_;
	id<MTLBuffer> fullscreenTexturedQuad_;
	ShaderStructures::DeferredBuffers deferredBuffers_;
	id<MTLTexture> deferredDebugColorAttachments_[FrameCount];

	dispatch_semaphore_t frameBoundarySemaphore_;
	uint32_t currentFrameIndex_;

	CBFrameAlloc frame_[FrameCount];
}

- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat {
	if (self = [super init]) {
		currentFrameIndex_ = 0;
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
			// create default sampler state
			MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
			samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
			samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
			samplerDesc.minFilter = MTLSamplerMinMagFilterNearest;
			samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
			samplerDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
			deferredSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
		}

		struct PosUV {
			simd::float2 pos;
			simd::float2 uv;
		};
		// top: uv.y = 0
		PosUV quad[] = {{{-1., -1.f}, {0.f, 1.f}}, {{-1., 1.f}, {0.f, 0.f}}, {{1., -1.f}, {1.f, 1.f}}, {{1., 1.f}, {1.f, 0.f}}};
		fullscreenTexturedQuad_ = [device_ newBufferWithBytes:quad length:sizeof(quad) options: MTLResourceOptionCPUCacheModeDefault];
		for (int i = 0; i < FrameCount; ++i)
			frame_[i].Init(device, defaultCBFrameAllocSize);
	}
	return self;
}
- (void)makeColorAttachmentTextures: (NSUInteger)width withHeight: (NSUInteger)height {
	if (!self->colorAttachmentTextures_[0][0] || [self->colorAttachmentTextures_[0][0] width] != width ||
		[self->colorAttachmentTextures_[0][0] height] != height) {
		for (int i = 0; i < FrameCount; ++i) {
			for (int j = 0; j < RenderTargetCount; ++j) {
				MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:[shaders_ getColorAttachmentFormats][j]
																								width:width
																							   height:height
																							mipmapped:NO];
				desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
				desc.resourceOptions = MTLResourceStorageModePrivate;
				self->colorAttachmentTextures_[i][j] = [device_ newTextureWithDescriptor:desc];
				[self->colorAttachmentTextures_[i][j] setLabel:@"Color Attachment"];
			}
			// debug...
			MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: MTLPixelFormatRGBA32Float
																								   width:width
																								   height:height
																								   mipmapped:NO];
			desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			desc.resourceOptions = MTLResourceStorageModePrivate;
			self->deferredDebugColorAttachments_[i] = [device_ newTextureWithDescriptor:desc];
			[self->deferredDebugColorAttachments_[i]  setLabel:@"Deferred debug color attachment"];
		}
	}
}
- (void)makeDepthTexture: (NSUInteger)width withHeight: (NSUInteger)height {
	if (!self->depthTextures_[0] || [self->depthTextures_[0] width] != width ||
		[self->depthTextures_[0] height] != height) {
		MTLPixelFormat pf = MTLPixelFormatDepth32Float;
		MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pf
																						width:width
																					   height:height
																					mipmapped:NO];
		desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		desc.resourceOptions = MTLResourceStorageModePrivate;
		for (int i = 0; i < FrameCount; ++i) {
			id<MTLTexture> texture = [device_ newTextureWithDescriptor:desc];
			[texture setLabel:@"Depth Texture"];
			self->depthTextures_[i] = texture;
		}
	}
}
-(BufferIndex) createBuffer: (size_t) length {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithLength: length options: MTLResourceCPUCacheModeWriteCombined];
	buffers_.push_back(mtlBuffer);
	return (BufferIndex)buffers_.size() - 1;
}

-(BufferIndex) createBuffer: (const void* _Nonnull) data withLength: (size_t) length {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithBytes:data length:length options: MTLResourceCPUCacheModeWriteCombined];
	buffers_.push_back(mtlBuffer);
	return (BufferIndex)buffers_.size() - 1;
}
- (TextureIndex) createTexture: (const void*_Nullable) data withWidth: (uint64_t) width withHeight: (uint32_t) height withBytesPerPixel: (uint8_t) bytesPerPixel withPixelFormat: (MTLPixelFormat) format withLabel: (NSString* _Nullable) label{
	MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
																								 width:width
																								height:height
																							 mipmapped:false];

	textureDescriptor.usage = MTLTextureUsageShaderRead;
	id<MTLTexture> texture = [device_ newTextureWithDescriptor:textureDescriptor];
	if (label) [texture setLabel:label];
	textures_.push_back({texture, width, height, bytesPerPixel, format});
	if (data) {
		MTLRegion region = MTLRegionMake2D(0, 0, width, height);
		const NSUInteger bytesPerRow = bytesPerPixel * width;
		[texture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:bytesPerRow];
	}
	return (TextureIndex)textures_.size() - 1;
}
- (TextureIndex) genCubeMap: (TextureIndex) tex
							  withVB: (BufferIndex) vb
							  withIB: (BufferIndex) ib
						 withSubmesh: (const assets::Submesh&) submesh
					   withDimension: (NSUInteger) dim
						  withShader: (ShaderId) shader
						   withLabel: (NSString* _Nullable) label {
	constexpr MTLPixelFormat format = MTLPixelFormatRGBA16Float;
	constexpr uint8_t bytesPerPixel = 8;
	MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat: format
																									size: dim
																							   mipmapped: false];

	textureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
	id<MTLTexture> cubeTexture = [device_ newTextureWithDescriptor:textureDescriptor];
	if (label) [cubeTexture setLabel:label];
	textures_.push_back({cubeTexture, dim, (uint32_t)dim, bytesPerPixel, format});

	const glm::vec3 at[] = {{1.f, 0.f, 0.f},{-1.f, 0.f, 0.f},{0.f, 1.f, 0.f},{0.f, -1.f, 0.f},{0.f, 0.f, -1.f},{0.f, 0.f, 1.f}},
		up[] = {{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, -1.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}};
	const int count = 6, inc = AlignTo<int, 256>(sizeof(glm::mat4x4));
	id<MTLBuffer> buffer = [device_ newBufferWithLength: inc * count options: MTLResourceCPUCacheModeWriteCombined];
	auto ptr = (uint8_t*)[buffer contents];
	for (int i = 0; i < count; ++i) {
		glm::mat4x4 v = glm::lookAt({0.f, 0.f, 0.f}, at[i], up[i] * -1.f/* TODO:: why???*/);
		memcpy(ptr, &v, sizeof(v));
		ptr += inc;
	}
	MTLCaptureManager* capManager = [MTLCaptureManager sharedCaptureManager];
	[capManager startCaptureWithCommandQueue:commandQueue_];

	id<MTLCommandBuffer> commandBuffer = [commandQueue_ commandBuffer];
	auto& pipeline = [shaders_ selectPipeline: shader];
	// TODO:: do the same with 1 drawcall and mrt
	for (int i = 0, offset = 0; i < count; ++i, offset += inc) {
		MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
		passDescriptor.colorAttachments[0].texture = cubeTexture;
		passDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
		passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
		passDescriptor.colorAttachments[0].slice = i;
		id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor: passDescriptor];
		[encoder setRenderPipelineState: pipeline.pipeline];
		[encoder setCullMode: MTLCullModeNone];
		[encoder setVertexBuffer: buffers_[vb] offset: 0 atIndex: 0];
		[encoder setVertexBuffer: buffer offset: offset atIndex: 1];
		[encoder setFragmentTexture: textures_[tex].texture atIndex:0];
		[encoder setFragmentSamplerState: self->defaultSamplerState_ atIndex:0];

		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[ib] indexBufferOffset: 0 instanceCount: 1 baseVertex: 0 baseInstance: 0];
		[encoder endEncoding];

	}
	[commandBuffer commit];
	[commandBuffer waitUntilCompleted];

	[capManager stopCapture];

	return (TextureIndex)textures_.size() - 1;
}
- (void) beginRender: (id<MTLTexture> _Nonnull) surface {
	dispatch_semaphore_wait(frameBoundarySemaphore_, DISPATCH_TIME_FOREVER);
	[self makeDepthTexture: surface.width withHeight: surface.height];
	[self makeColorAttachmentTextures: surface.width withHeight: surface.height];
	// TODO:: storing the surface currently redundant
	surface_ = surface;
	commandBuffers_.reserve([shaders_ getPipelineCount]);
	for (size_t i = 0; i < [shaders_ getPipelineCount]; ++i)
		commandBuffers_.push_back([commandQueue_ commandBuffer]);
	encoders_.resize(commandBuffers_.size());
	frame_[currentFrameIndex_].Reset();
}
- (void) startForwardPass {
	MTLRenderPassDescriptor *firstPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	firstPassDescriptor.colorAttachments[0].texture = surface_;
	firstPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
	firstPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	firstPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.f, 0., 0., 0.f);
//	firstPassDescriptor.depthAttachment.texture = depthTextures_[currentFrameIndex_];
//	firstPassDescriptor.depthAttachment.clearDepth = 1.f;
//	firstPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
//	firstPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;

	MTLRenderPassDescriptor *followingPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	followingPassDescriptor.colorAttachments[0].texture = surface_;
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
- (void) startDeferredPass {
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
		if (pipeline.renderPass == RenderPass::Deferred) {
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

//- (void) submitToEncoder: (size_t) encoderIndex withPipeline: (size_t) pipelineIndex vsUniforms: (const std::vector<size_t>&) vsBuffers fsUniforms: (const std::vector<size_t>&) fsBuffers withMesh: (const Mesh&)mesh {
//	id<MTLRenderCommandEncoder> commandEncoder = encoders_[encoderIndex];
//
//	size_t vsAttribIndex = 0, fsAttribIndex = 0;
//	// attribs
//	[commandEncoder setVertexBuffer: buffers_[mesh.vb].buffer offset: 0 atIndex: vsAttribIndex++];
//	// TODO:: if (mesh.nb != InvalidBufferIndex) [commandEncoder setVertexBuffer: buffers_[mesh.nb].buffer offset: 0 atIndex: vsAttribIndex++];
//
//	// uniforms
//	for (const auto& index : vsBuffers) {
//		[commandEncoder setVertexBuffer: buffers_[index].buffer offset: 0 atIndex: vsAttribIndex++];
//	}
//	for (const auto& index : fsBuffers) {
//		[commandEncoder setFragmentBuffer: buffers_[index].buffer offset: 0 atIndex: fsAttribIndex++];
//	}
//	for (const auto& layer : mesh.layers)
//		for (const auto& submesh : layer.submeshes) {
////	TODO::
////			if (submesh.material.color_uvb ! = InvalidBufferIndex)
////				[commandEncoder setVertexBuffer: buffers_[submesh.material.color_uvb].buffer offset: 0 atIndex: attribIndex++];
//			[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[mesh.ib].buffer indexBufferOffset: submesh.offset instanceCount: 1 baseVertex: 0 baseInstance: 0];
//		}
//}

- (void) renderTo: (id<CAMetalDrawable> _Nonnull) drawable {




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
	for (auto commandBuffer : commandBuffers_) {
		[commandBuffer commit];
	}

	id<MTLCommandBuffer> deferredCommandBuffer = [commandQueue_ commandBuffer];
	MTLRenderPassDescriptor *deferredPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	deferredPassDescriptor.colorAttachments[0].texture = drawable.texture;
	deferredPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	deferredPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	//debug...
	deferredPassDescriptor.colorAttachments[1].texture = deferredDebugColorAttachments_[currentFrameIndex_];
	deferredPassDescriptor.colorAttachments[1].loadAction = MTLLoadActionDontCare;
	deferredPassDescriptor.colorAttachments[1].storeAction = MTLStoreActionStore;

	id<MTLRenderCommandEncoder> deferredEncoder = [deferredCommandBuffer renderCommandEncoderWithDescriptor: deferredPassDescriptor];
	[deferredEncoder setCullMode: MTLCullModeBack];
	[deferredEncoder setRenderPipelineState: [shaders_ selectPipeline: DeferredPBR].pipeline];
	[deferredEncoder setVertexBuffer: fullscreenTexturedQuad_ offset: 0 atIndex: 0];
	NSUInteger fsTexIndex = 0;
	for (; fsTexIndex < RenderTargetCount; ++fsTexIndex) {
		[deferredEncoder setFragmentTexture: colorAttachmentTextures_[currentFrameIndex_][fsTexIndex] atIndex:fsTexIndex];
	}
	[deferredEncoder setFragmentTexture: depthTextures_[currentFrameIndex_] atIndex:fsTexIndex++];
	assert(deferredBuffers_.irradiance != InvalidTexture);
	[deferredEncoder setFragmentTexture: textures_[deferredBuffers_.irradiance].texture atIndex:fsTexIndex++];
	[deferredEncoder setFragmentSamplerState:self->deferredSamplerState_ atIndex:0];

	// uniforms
	size_t vsIndex = 0, fsIndex = 0;
	for (const auto& info : deferredBuffers_.vsBuffers) {
		auto offset = currentFrameIndex_ % info.bufferCount;
		[deferredEncoder setVertexBuffer: buffers_[info.bufferIndex + offset] offset: 0 atIndex: vsIndex++];
	}
	for (const auto& info : deferredBuffers_.fsBuffers) {
		auto offset = currentFrameIndex_ % info.bufferCount;
		[deferredEncoder setFragmentBuffer: buffers_[info.bufferIndex + offset] offset: 0 atIndex: fsIndex++];
	}

	[deferredEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart: 0 vertexCount: 4 instanceCount: 1 baseInstance: 0];

	[deferredEncoder endEncoding];
	[deferredCommandBuffer presentDrawable:drawable];
	__weak dispatch_semaphore_t semaphore = frameBoundarySemaphore_;
	[deferredCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
		// GPU work is complete
		// Signal the semaphore to start the CPU work
		dispatch_semaphore_signal(semaphore);
	}];
	[deferredCommandBuffer commit];
	// TODO: is this necessary?
	encoders_.clear();
	// TODO: is this necessary?
	commandBuffers_.clear();
	currentFrameIndex_ = (currentFrameIndex_ + 1) % ShaderStructures::FrameCount;
	//MTLCaptureManager* capManager = [MTLCaptureManager sharedCaptureManager];
	//[capManager stopCapture];
}

- (uint32_t) getCurrentFrameIndex {
	return currentFrameIndex_;
}

- (ShaderResourceIndex) createShaderResource: (uint32_t) length withCount: (uint16_t) count {
	// TODO:: create stack allocator here as well
	length = AlignTo<decltype(length), 16>(length);
	for (uint16_t i = 0; i < count; ++i) {
		id<MTLBuffer> buffer = [device_ newBufferWithLength: length options: MTLResourceOptionCPUCacheModeDefault];
		buffers_.push_back(buffer);
	}
	return (ShaderResourceIndex)buffers_.size() - count;
}

- (void) updateShaderResource: (ShaderResourceIndex)shaderResourceIndex withData: (const void* _Null_unspecified)data andSize: (size_t)size {
	// TODO:: currently one buffer for each uniform
	memcpy([buffers_[shaderResourceIndex] contents], data, size);
}

-(void) submitDebugCmd: (const ShaderStructures::DebugCmd&) cmd {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[ShaderStructures::Debug];

	size_t vsAttribIndex = 0, fsAttribIndex = 0;
	// attribs
	[commandEncoder setVertexBuffer: buffers_[cmd.vb] offset: 0 atIndex: vsAttribIndex++];

	// uniforms
	for (const auto& info : cmd.vsBuffers) {
		auto offset = currentFrameIndex_ % info.bufferCount;
		[commandEncoder setVertexBuffer: buffers_[info.bufferIndex + offset] offset: 0 atIndex: vsAttribIndex++];
	}
	for (const auto& info : cmd.fsBuffers) {
		auto offset = currentFrameIndex_ % info.bufferCount;
		[commandEncoder setFragmentBuffer: buffers_[info.bufferIndex + offset] offset: 0 atIndex: fsAttribIndex++];
	}
	if (cmd.ib != InvalidBuffer)
		[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib] indexBufferOffset: cmd.offset instanceCount: 1 baseVertex: 0 baseInstance: 0];
	else
		[commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart: cmd.offset vertexCount: cmd.count instanceCount: 1 baseInstance: 0];
}

-(void) submitPosCmd: (const ShaderStructures::PosCmd&) cmd {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[ShaderStructures::Pos];

	size_t vsAttribIndex = 0, fsAttribIndex = 0;
	// attribs
	[commandEncoder setVertexBuffer: buffers_[cmd.vb] offset: 0 atIndex: vsAttribIndex++];
	[commandEncoder setVertexBuffer: buffers_[cmd.nb] offset: 0 atIndex: vsAttribIndex++];

	// uniforms
	for (const auto& info : cmd.vsBuffers) {
		auto offset = currentFrameIndex_ % info.bufferCount;
		[commandEncoder setVertexBuffer: buffers_[info.bufferIndex + offset] offset: 0 atIndex: vsAttribIndex++];
	}
	for (const auto& info : cmd.fsBuffers) {
		auto offset = currentFrameIndex_ % info.bufferCount;
		[commandEncoder setFragmentBuffer: buffers_[info.bufferIndex + offset] offset: 0 atIndex: fsAttribIndex++];
	}
	if (cmd.ib != InvalidBuffer)
		[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib] indexBufferOffset: cmd.offset instanceCount: 1 baseVertex: 0 baseInstance: 0];
	else
		[commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart: cmd.offset vertexCount: cmd.count instanceCount: 1 baseInstance: 0];
}

-(void) submitTexCmd: (const ShaderStructures::TexCmd&) cmd {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[ShaderStructures::Tex];

	size_t vsAttribIndex = 0, fsAttribIndex = 0;
	// attribs
	[commandEncoder setVertexBuffer: buffers_[cmd.vb] offset: 0 atIndex: vsAttribIndex++];
	[commandEncoder setVertexBuffer: buffers_[cmd.nb] offset: 0 atIndex: vsAttribIndex++];
	[commandEncoder setVertexBuffer: buffers_[cmd.uvb] offset: 0 atIndex: vsAttribIndex++];

	// uniforms
	for (const auto& info : cmd.vsBuffers) {
		auto offset = currentFrameIndex_ % info.bufferCount;
		[commandEncoder setVertexBuffer: buffers_[info.bufferIndex + offset] offset: 0 atIndex: vsAttribIndex++];
	}
	for (const auto& info : cmd.fsBuffers) {
		auto offset = currentFrameIndex_ % info.bufferCount;
		[commandEncoder setFragmentBuffer: buffers_[info.bufferIndex + offset] offset: 0 atIndex: fsAttribIndex++];
	}
	NSUInteger atIndex = 0;
	for (const auto& textureIndex : cmd.textures) {
		[commandEncoder setFragmentTexture: textures_[textureIndex].texture atIndex:atIndex++];
	}
	[commandEncoder setFragmentSamplerState:self->defaultSamplerState_ atIndex:0];

	if (cmd.ib != InvalidBuffer)
		[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib] indexBufferOffset: cmd.offset instanceCount: 1 baseVertex: 0 baseInstance: 0];
	else
		[commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart: cmd.offset vertexCount: cmd.count instanceCount: 1 baseInstance: 0];
}

-(void) submitBgCmd: (const ShaderStructures::BgCmd&) cmd {
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
		[commandEncoder setFragmentSamplerState:self->defaultSamplerState_ atIndex:0];
	}
	[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib] indexBufferOffset: cmd.submesh.ibByteOffset instanceCount: 1 baseVertex: 0 baseInstance: 0];
}

-(void) submitDrawCmd: (const ShaderStructures::DrawCmd&) cmd {
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
				memcpy(cb.address, &cmd.material.albedo, sizeof(float4));
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			break;
		}
		case ShaderStructures::Pos: {
			{
				auto cb = frame.Alloc(sizeof(ShaderStructures::Object));
				((ShaderStructures::Object*)cb.address)->m = cmd.m;
				((ShaderStructures::Object*)cb.address)->mvp = cmd.mvp;
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			{
				auto cb = frame.Alloc(sizeof(ShaderStructures::Material));
				((ShaderStructures::Material*)cb.address)->diffuse = cmd.material.albedo;
				((ShaderStructures::Material*)cb.address)->specular_power = {cmd.material.metallic, cmd.material.roughness};
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			break;
		}
		case ShaderStructures::Tex: {
			{
				auto cb = frame.Alloc(sizeof(ShaderStructures::Object));
				((ShaderStructures::Object*)cb.address)->m = cmd.m;
				((ShaderStructures::Object*)cb.address)->mvp = cmd.mvp;
				[commandEncoder setVertexBuffer: cb.buffer offset: cb.offset atIndex: vsAttribIndex++];
			}
			{
				auto& texture = textures_[cmd.material.texAlbedo];
				[commandEncoder setFragmentTexture: texture.texture atIndex:textureIndex++];
				[commandEncoder setFragmentSamplerState:self->defaultSamplerState_ atIndex:0];
			}
			{
				auto cb = frame.Alloc(sizeof(ShaderStructures::Material));
				((ShaderStructures::Material*)cb.address)->diffuse = cmd.material.albedo;
				((ShaderStructures::Material*)cb.address)->specular_power = {cmd.material.metallic, cmd.material.roughness};
				[commandEncoder setFragmentBuffer: cb.buffer offset: cb.offset atIndex: fsAttribIndex++];
			}
			break;
		}
		default: assert(false);
	}
	[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib] indexBufferOffset: cmd.submesh.ibByteOffset instanceCount: 1 baseVertex: 0 baseInstance: 0];
}

-(void) setDeferredBuffers: (const ShaderStructures::DeferredBuffers&) buffers {
	deferredBuffers_ = buffers;
}
@end

