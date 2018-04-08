#import "Renderer.h"
#import "Shaders.h"
#import "cpp/RendererWrapper.h"
#include <array>
#include "cpp/Assets.hpp"
#include "cpp/BufferUtils.h"

const int RendererWrapper::frameCount_ = 1;

void RendererWrapper::Init(void* renderer) {
	this->renderer = renderer;
}
BufferIndex RendererWrapper::CreateBuffer(const void* buffer, size_t length, size_t elementSize) {
	return [(__bridge Renderer*)renderer createBuffer:buffer withLength:length withElementSize:elementSize];
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
namespace {
	MTLPixelFormat PixelFormatToMTLPixelFormat(Img::PixelFormat format) {
		switch (format) {
			case Img::PixelFormat::RGBA8:
				return MTLPixelFormatRGBA8Unorm;
			case Img::PixelFormat::BGRA8:
				return MTLPixelFormatBGRA8Unorm;
			default:
				assert(false);
		}
		return MTLPixelFormatInvalid;
	}
}
TextureIndex RendererWrapper::CreateTexture(const void* buffer, uint64_t width, uint32_t height, uint8_t bytesPerPixel, Img::PixelFormat format) {
	return [(__bridge Renderer*)renderer createTexture: buffer withWidth: width withHeight: height withBytesPerPixel: bytesPerPixel withPixelFormat: PixelFormatToMTLPixelFormat(format)];
}
void RendererWrapper::BeginRender() {
	[(__bridge Renderer*)renderer beginRender];
}
size_t RendererWrapper::StartRenderPass() {
	// TODO::
	//return 	[(__bridge Renderer*)renderer startRenderPass:];
	return 0u;
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

struct Buffer {
	id<MTLBuffer> buffer;
	size_t size, elementSize;
};
struct Texture {
	id<MTLTexture> texture;
	uint64_t width;
	uint32_t height;
	uint8_t bytesPerPixel;
	MTLPixelFormat format;

};
@implementation Renderer
{
	id<MTLDevice> device_;
	id<MTLCommandQueue> commandQueue_;
	id<MTLTexture> depthTexture_;
	std::vector<Buffer> buffers_;
	std::vector<Texture> textures_;
	std::vector<id<MTLRenderCommandEncoder>> encoders_;
	std::vector<id<MTLCommandBuffer>> commandBuffers_;
	Shaders* shaders_;
	id<MTLDepthStencilState> depthStencilState_;
	id<MTLSamplerState> defaultSamplerState_;
	uint32_t currentFrameIndex_;
}

- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat {
	if (self = [super init]) {
		currentFrameIndex_ = 0;
		device_ = device;
		commandQueue_ = [device newCommandQueue];
		shaders_ = [[Shaders alloc] initWithDevice:device andPixelFormat:pixelFormat];

		//[self makeDepthTexture];

		MTLDepthStencilDescriptor *depthStencilDescriptor = [MTLDepthStencilDescriptor new];
		depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLess;
		depthStencilDescriptor.depthWriteEnabled = YES;
		depthStencilState_ = [device newDepthStencilStateWithDescriptor:depthStencilDescriptor];

		// create default sampler state
		MTLSamplerDescriptor *samplerDesc = [MTLSamplerDescriptor new];
		samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.minFilter = MTLSamplerMinMagFilterNearest;
		samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.mipFilter = MTLSamplerMipFilterLinear;
		defaultSamplerState_ = [device newSamplerStateWithDescriptor:samplerDesc];
	}
	return self;
}

- (void)makeDepthTexture: (NSUInteger)width withHeight: (NSUInteger)height {
	if (!self->depthTexture_ || [self->depthTexture_ width] != width ||
		[self->depthTexture_ height] != height) {
		MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
																						width:width
																					   height:height
																					mipmapped:NO];
		desc.usage = MTLTextureUsageRenderTarget;
		desc.resourceOptions = MTLResourceStorageModePrivate;
		self->depthTexture_ = [device_ newTextureWithDescriptor:desc];
	}
}
-(BufferIndex) createBuffer: (size_t) length {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithLength: length options: MTLResourceOptionCPUCacheModeDefault];
	buffers_.push_back({mtlBuffer, length, 1});
	return (BufferIndex)buffers_.size() - 1;
}

-(BufferIndex) createBuffer: (const void* _Nonnull) buffer withLength: (size_t) length withElementSize: (size_t) elementSize {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithBytes:buffer length:length options: MTLResourceOptionCPUCacheModeDefault];
	buffers_.push_back({mtlBuffer, length, elementSize});
	return (BufferIndex)buffers_.size() - 1;
}
- (TextureIndex) createTexture: (const void* _Nonnull) buffer withWidth: (uint64_t) width withHeight: (uint32_t) height withBytesPerPixel: (uint8_t) bytesPerPixel withPixelFormat: (MTLPixelFormat) format {
	MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
																								 width:width
																								height:height
																							 mipmapped:false];

	textureDescriptor.usage = MTLTextureUsageShaderRead;
	id<MTLTexture> texture = [device_ newTextureWithDescriptor:textureDescriptor];
	// TODO::
	//[texture setLabel:imageName];
	textures_.push_back({texture, width, height, bytesPerPixel, format});
	MTLRegion region = MTLRegionMake2D(0, 0, width, height);
	const NSUInteger bytesPerRow = bytesPerPixel * width;
	[texture replaceRegion:region mipmapLevel:0 withBytes:buffer bytesPerRow:bytesPerRow];
	return (TextureIndex)textures_.size() - 1;
}
- (void) beginRender {
	commandBuffers_.reserve([shaders_ getPipelineCount]);
	for (size_t i = 0; i < [shaders_ getPipelineCount]; ++i)
		commandBuffers_.push_back([commandQueue_ commandBuffer]);
}
- (void) startRenderPass: (id<MTLTexture> _Nonnull) texture {
	[self makeDepthTexture:texture.width withHeight:texture.height];
	MTLRenderPassDescriptor *firstPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	firstPassDescriptor.colorAttachments[0].texture = texture;
	firstPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
	firstPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	firstPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(.5, .5, .5, 1.);

	firstPassDescriptor.depthAttachment.texture = depthTexture_;
	firstPassDescriptor.depthAttachment.clearDepth = 1.0;
	firstPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
	firstPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;

	MTLRenderPassDescriptor *followingPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	followingPassDescriptor.colorAttachments[0].texture = texture;
	followingPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	followingPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

	followingPassDescriptor.depthAttachment.texture = depthTexture_;
	followingPassDescriptor.depthAttachment.clearDepth = 1.0;
	followingPassDescriptor.depthAttachment.loadAction = MTLLoadActionDontCare;
	followingPassDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;

	for (size_t i = 0; i < [shaders_ getPipelineCount]; ++i) {
		auto encoder = [commandBuffers_[i] renderCommandEncoderWithDescriptor: (i == 0) ? firstPassDescriptor : followingPassDescriptor];
		encoders_.push_back(encoder);
		[encoder setRenderPipelineState: [shaders_ selectPipeline: i]];
		[encoder setDepthStencilState: depthStencilState_];
	};
}

- (void) submitToEncoder: (size_t) encoderIndex withPipeline: (size_t) pipelineIndex vsUniforms: (const std::vector<size_t>&) vsBuffers fsUniforms: (const std::vector<size_t>&) fsBuffers withMesh: (const Mesh&)mesh {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[encoderIndex];

	size_t vsAttribIndex = 0, fsAttribIndex = 0;
	// attribs
	[commandEncoder setVertexBuffer: buffers_[mesh.vb].buffer offset: 0 atIndex: vsAttribIndex++];
	// TODO:: if (mesh.nb != InvalidBufferIndex) [commandEncoder setVertexBuffer: buffers_[mesh.nb].buffer offset: 0 atIndex: vsAttribIndex++];

	// uniforms
	for (const auto& index : vsBuffers) {
		[commandEncoder setVertexBuffer: buffers_[index].buffer offset: 0 atIndex: vsAttribIndex++];
	}
	for (const auto& index : fsBuffers) {
		[commandEncoder setFragmentBuffer: buffers_[index].buffer offset: 0 atIndex: fsAttribIndex++];
	}
	for (const auto& layer : mesh.layers)
		for (const auto& submesh : layer.submeshes) {
//	TODO::
//			if (submesh.material.color_uvb ! = InvalidBufferIndex)
//				[commandEncoder setVertexBuffer: buffers_[submesh.material.color_uvb].buffer offset: 0 atIndex: attribIndex++];
			[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: submesh.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[mesh.ib].buffer indexBufferOffset: submesh.offset instanceCount: 1 baseVertex: 0 baseInstance: 0];
		}
}

- (void) renderTo: (id<CAMetalDrawable> _Nonnull) drawable {
	for (auto commandEncoder : encoders_) {
		[commandEncoder endEncoding];
	}
	for (auto commandBuffer : commandBuffers_) {
		[commandBuffer commit];
	}
	assert(!commandBuffers_.empty());
	[commandBuffers_.back() waitUntilCompleted];
	[drawable present];
	encoders_.clear();
	commandBuffers_.clear();
	++currentFrameIndex_;
}

- (uint32_t) getCurrentFrameIndex {
	return currentFrameIndex_ % RendererWrapper::frameCount_;
}

- (ShaderResourceIndex) createShaderResource: (uint32_t) length withCount: (uint16_t) count {
	// TODO:: create stack allocator here as well
	length = AlignTo<decltype(length), 16>(length);
	for (uint16_t i = 0; i < count; ++i) {
		id<MTLBuffer> mtlBuffer = [device_ newBufferWithLength: length options: MTLResourceOptionCPUCacheModeDefault];
		buffers_.push_back({mtlBuffer, length, 1});
	}
	return (ShaderResourceIndex)buffers_.size() - count;
}

- (void) updateShaderResource: (ShaderResourceIndex)shaderResourceIndex withData: (const void* _Null_unspecified)data andSize: (size_t)size {
	// TODO:: currently one buffer for each uniform
	memcpy([buffers_[shaderResourceIndex].buffer contents], data, size);
}

-(void) submitDebugCmd: (const ShaderStructures::DebugCmd&) cmd {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[ShaderStructures::Debug];

	size_t vsAttribIndex = 0, fsAttribIndex = 0;
	// attribs
	[commandEncoder setVertexBuffer: buffers_[cmd.vb].buffer offset: 0 atIndex: vsAttribIndex++];

	// uniforms
	for (const auto& info : cmd.vsBuffers) {
		auto offset = ([self getCurrentFrameIndex] + 1) % info.bufferCount;
		[commandEncoder setVertexBuffer: buffers_[info.bufferIndex + offset].buffer offset: 0 atIndex: vsAttribIndex++];
	}
	for (const auto& info : cmd.fsBuffers) {
		auto offset = ([self getCurrentFrameIndex] + 1) % info.bufferCount;
		[commandEncoder setFragmentBuffer: buffers_[info.bufferIndex + offset].buffer offset: 0 atIndex: fsAttribIndex++];
	}
	if (cmd.ib != InvalidBuffer)
		[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib].buffer indexBufferOffset: cmd.offset instanceCount: 1 baseVertex: 0 baseInstance: 0];
	else
		[commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart: cmd.offset vertexCount: cmd.count instanceCount: 1 baseInstance: 0];
}


-(void) submitPosCmd: (const ShaderStructures::PosCmd&) cmd {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[ShaderStructures::Pos];

	size_t vsAttribIndex = 0, fsAttribIndex = 0;
	// attribs
	[commandEncoder setVertexBuffer: buffers_[cmd.vb].buffer offset: 0 atIndex: vsAttribIndex++];
	[commandEncoder setVertexBuffer: buffers_[cmd.nb].buffer offset: 0 atIndex: vsAttribIndex++];

	// uniforms
	for (const auto& info : cmd.vsBuffers) {
		auto offset = ([self getCurrentFrameIndex] + 1) % info.bufferCount;
		[commandEncoder setVertexBuffer: buffers_[info.bufferIndex + offset].buffer offset: 0 atIndex: vsAttribIndex++];
	}
	for (const auto& info : cmd.fsBuffers) {
		auto offset = ([self getCurrentFrameIndex] + 1) % info.bufferCount;
		[commandEncoder setFragmentBuffer: buffers_[info.bufferIndex + offset].buffer offset: 0 atIndex: fsAttribIndex++];
	}
	if (cmd.ib != InvalidBuffer)
		[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib].buffer indexBufferOffset: cmd.offset instanceCount: 1 baseVertex: 0 baseInstance: 0];
	else
		[commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart: cmd.offset vertexCount: cmd.count instanceCount: 1 baseInstance: 0];
}

-(void) submitTexCmd: (const ShaderStructures::TexCmd&) cmd {
	MTLCaptureManager* capManager = [MTLCaptureManager sharedCaptureManager];
	[capManager startCaptureWithCommandQueue: commandQueue_];
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[ShaderStructures::Tex];

	size_t vsAttribIndex = 0, fsAttribIndex = 0;
	// attribs
	[commandEncoder setVertexBuffer: buffers_[cmd.vb].buffer offset: 0 atIndex: vsAttribIndex++];
	[commandEncoder setVertexBuffer: buffers_[cmd.nb].buffer offset: 0 atIndex: vsAttribIndex++];
	[commandEncoder setVertexBuffer: buffers_[cmd.uvb].buffer offset: 0 atIndex: vsAttribIndex++];

	// uniforms
	for (const auto& info : cmd.vsBuffers) {
		auto offset = ([self getCurrentFrameIndex] + 1) % info.bufferCount;
		[commandEncoder setVertexBuffer: buffers_[info.bufferIndex + offset].buffer offset: 0 atIndex: vsAttribIndex++];
	}
	for (const auto& info : cmd.fsBuffers) {
		auto offset = ([self getCurrentFrameIndex] + 1) % info.bufferCount;
		[commandEncoder setFragmentBuffer: buffers_[info.bufferIndex + offset].buffer offset: 0 atIndex: fsAttribIndex++];
	}
	NSUInteger atIndex = 0;
	for (const auto& textureIndex : cmd.textures) {
		[commandEncoder setFragmentTexture: textures_[textureIndex].texture atIndex:atIndex++];
	}
	[commandEncoder setFragmentSamplerState:self->defaultSamplerState_ atIndex:0];

	if (cmd.ib != InvalidBuffer)
		[commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount: cmd.count indexType: MTLIndexTypeUInt16 indexBuffer: buffers_[cmd.ib].buffer indexBufferOffset: cmd.offset instanceCount: 1 baseVertex: 0 baseInstance: 0];
	else
		[commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart: cmd.offset vertexCount: cmd.count instanceCount: 1 baseInstance: 0];

//	[capManager stopCapture];
}
@end

