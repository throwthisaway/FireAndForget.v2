#import "Renderer.h"
#import "Shaders.h"
#import "cpp/RendererWrapper.h"
#include <array>
#include "cpp/Assets.hpp"
#include "cpp/ShaderStructures.h"

const int RendererWrapper::frameCount_ = 1;

void RendererWrapper::Init(void* renderer) {
	this->renderer = renderer;
}
size_t RendererWrapper::CreateBuffer(const void* buffer, size_t length, size_t elementSize) {
	return [(__bridge Renderer*)renderer createBuffer:buffer withLength:length withElementSize:elementSize];
}

void RendererWrapper::SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, ResourceHeapHandle shaderResourceHeap, const std::vector<size_t>& vsBuffers, const std::vector<size_t>& psBuffers, const Mesh& mesh) {
	[(__bridge Renderer*)renderer submitToEncoder:encoderIndex withPipeline: pipelineIndex vsUniforms: vsBuffers fsUniforms: psBuffers withMesh: mesh];
}

BufferIndex RendererWrapper::CreateTexture(const void* buffer, uint64_t width, uint32_t height, uint8_t bytesPerPixel, Img::PixelFormat format) {
	// TODO::
	return InvalidBufferIndex;
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
ResourceHeapHandle RendererWrapper::GetStaticShaderResourceHeap(unsigned short descCountNeeded) {
	return [(__bridge Renderer*)renderer getStaticShaderResourceHeap];
}
ShaderResourceIndex RendererWrapper::GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, size_t size, unsigned short count) {
	return [(__bridge Renderer*)renderer getShaderResourceIndexFromHeap: shaderResourceHeap withSize: size andCount: count];
}
void RendererWrapper::UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size) {
	return [(__bridge Renderer*)renderer updateShaderResource: shaderResourceIndex withData: data andSize: size];
}
ShaderResourceIndex RendererWrapper::GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, BufferIndex textureIndex) {
	return [(__bridge Renderer*)renderer getShaderResourceIndexFromHeap: shaderResourceHeap withTextureIndex: textureIndex];
}

struct Buffer {
	id<MTLBuffer> buffer;
	size_t size, elementSize;
};
@implementation Renderer
{
	id <MTLDevice> device_;
	id <MTLCommandQueue> commandQueue_;
	std::vector<Buffer> buffers_;
	std::vector<id<MTLRenderCommandEncoder>> encoders_;
	std::vector<id<MTLCommandBuffer>> commandBuffers_;
	Shaders* shaders_;
	uint32_t currentFrameIndex_;
}

- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat {
	if (self = [super init]) {
		currentFrameIndex_ = 0;
		device_ = device;
		commandQueue_ = [device newCommandQueue];
		shaders_ = [[Shaders alloc] initWithDevice:device andPixelFormat:pixelFormat];
	}
	return self;
}

-(BufferIndex) createBuffer: (size_t) length {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithLength: length options: MTLResourceOptionCPUCacheModeDefault];
	buffers_.push_back({mtlBuffer, length, 1});
	return buffers_.size() - 1;
}

-(BufferIndex) createBuffer: (const void* _Nonnull) buffer withLength: (size_t) length withElementSize: (size_t) elementSize {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithBytes:buffer length:length options: MTLResourceOptionCPUCacheModeDefault];
	buffers_.push_back({mtlBuffer, length, elementSize});
	return buffers_.size() - 1;
}
- (void) beginRender {
	commandBuffers_.reserve([shaders_ getPipelineCount]);
	for (size_t i = 0; i < [shaders_ getPipelineCount]; ++i)
		commandBuffers_.push_back([commandQueue_ commandBuffer]);
}
- (void) startRenderPass: (id<MTLTexture> _Nonnull) texture {
	MTLRenderPassDescriptor *firstPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	firstPassDescriptor.colorAttachments[0].texture = texture;
	firstPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
	firstPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	firstPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(.5, .5, .5, 1.);

	MTLRenderPassDescriptor *followingPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	followingPassDescriptor.colorAttachments[0].texture = texture;
	followingPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	followingPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

	for (size_t i = 0; i < [shaders_ getPipelineCount]; ++i) {
		auto encoder = [commandBuffers_[i] renderCommandEncoderWithDescriptor: (i == 0) ? firstPassDescriptor : followingPassDescriptor];
		encoders_.push_back(encoder);
		[encoder setRenderPipelineState: [shaders_ selectPipeline: i]];
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

- (ResourceHeapHandle) getStaticShaderResourceHeap {
	// TODO:: currently one buffer for each uniform
	return 0u;
}
- (ShaderResourceIndex) getShaderResourceIndexFromHeap: (ResourceHeapHandle)shaderResourceHeap withSize: (size_t)size andCount: (uint16_t) count {
	// TODO:: currently one buffer for each uniform
	auto res = [self createBuffer: size];
	for (uint16_t i = 1; i < count; ++i) {
		[self createBuffer: size];
	}
	return res;
}
- (void) updateShaderResource: (ShaderResourceIndex)shaderResourceIndex withData: (const void* _Null_unspecified)data andSize: (size_t)size {
	// TODO:: currently one buffer for each uniform
	memcpy([buffers_[shaderResourceIndex].buffer contents], data, size);
}
- (ShaderResourceIndex) getShaderResourceIndexFromHeap: (ResourceHeapHandle)shaderResourceHeap withTextureIndex: (BufferIndex)textureIndex {
	// TODO::
	return InvalidShaderResourceIndex;
}
@end

