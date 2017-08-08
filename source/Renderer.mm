#import "Renderer.h"
#import "Shaders.h"
#import "cpp/RendererWrapper.h"
#include <array>
#include "cpp/Materials.h"

void RendererWrapper::Init(void* self) {
	this->self = self;
}
size_t RendererWrapper::CreateBuffer(const void* buffer, size_t length, size_t elementSize) {
	return [(__bridge Renderer*)self createBuffer:buffer withLength:length withElementSize:elementSize];
}

void RendererWrapper::SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, uint8_t* uniforms, MeshDesc&& meshDesc) {
	[(__bridge Renderer*)self submitToEncoder:encoderIndex withPipeline: pipelineIndex withUniforms: uniforms withDesc: std::move(meshDesc)];
}

struct Buffer {
	id<MTLBuffer> buffer;
	size_t size, elementSize;
};
@implementation Renderer
{
	id <MTLDevice> device_;
	id <MTLCommandQueue> commandQueue_;
	//NSMutableArray<id<MTLBuffer>>* buffers_;
	std::vector<Buffer> buffers_;
	std::array<size_t, Materials::Count> uniforms_;
	std::vector<id<MTLRenderCommandEncoder>> encoders_;
	std::vector<id<MTLCommandBuffer>> commandBuffers_;
	Shaders* shaders_;
}

- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat {
	if (self = [super init]) {
		device_ = device;
		commandQueue_ = [device newCommandQueue];
		shaders_ = [[Shaders alloc] initWithDevice:device andPixelFormat:pixelFormat];
		//buffers_ = [[NSMutableArray alloc]init];

		uniforms_[Materials::ColPos] = [self createBuffer: sizeof(Materials::ColPosBuffer)];
	}
	return self;
}

-(size_t) createBuffer: (size_t) length {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithLength: length options: MTLResourceOptionCPUCacheModeDefault];
	buffers_.push_back({mtlBuffer, length, 1});
	return buffers_.size() - 1;
	// TODO:: buffers_ is immutable this way
//	[buffers_ addObject:mtlBuffer];
//	return [buffers_ count] - 1;
}

-(size_t) createBuffer: (const void* _Nonnull) buffer withLength: (size_t) length withElementSize: (size_t) elementSize {
	id<MTLBuffer> mtlBuffer = [device_ newBufferWithBytes:buffer length:length options: MTLResourceOptionCPUCacheModeDefault];
	buffers_.push_back({mtlBuffer, length, elementSize});
	// TODO:: buffers_ is immutable this way
//	[buffers_ addObject:mtlBuffer];
//	return [buffers_ count] - 1;
	return buffers_.size() - 1;
}
- (size_t) beginRender {
	commandBuffers_.push_back([commandQueue_ commandBuffer]);
	return commandBuffers_.size() - 1;
}
- (size_t) startRenderPass: (id<MTLTexture> _Nonnull) texture withCommandBuffer: (size_t) commandBufferIndex {
	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	passDescriptor.colorAttachments[0].texture = texture;
	passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(.5, .5, .5, 1.);

	id<MTLCommandBuffer> commandBuffer = commandBuffers_[commandBufferIndex];

	encoders_.push_back([commandBuffer renderCommandEncoderWithDescriptor: passDescriptor]);
	return encoders_.size() - 1;
}

- (void) submitToEncoder: (size_t) encoderIndex withPipeline: (size_t) pipelineIndex withUniforms: (uint8_t* _Nonnull) uniforms withDesc: (MeshDesc&&)meshDesc {
	id<MTLRenderCommandEncoder> commandEncoder = encoders_[encoderIndex];
	[commandEncoder setRenderPipelineState: [shaders_ selectPipeline:pipelineIndex]];

	size_t attribIndex = 0;
	for (const auto& bufferIndex : meshDesc.buffers) {
		[commandEncoder setVertexBuffer: buffers_[bufferIndex].buffer offset: meshDesc.offset * buffers_[bufferIndex].elementSize atIndex: attribIndex];
		++attribIndex;
	}
	memcpy([buffers_[uniforms_[pipelineIndex]].buffer contents], uniforms, buffers_[uniforms_[pipelineIndex]].size);
	[commandEncoder setVertexBuffer: buffers_[uniforms_[pipelineIndex]].buffer offset: 0 atIndex: attribIndex];
	++attribIndex;

	[commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:meshDesc.offset vertexCount:meshDesc.count];
}

- (void) renderTo: (id<CAMetalDrawable> _Nonnull) drawable withCommandBuffer: (size_t) commandBufferIndex {
	for (auto commandEncoder : encoders_) {
		[commandEncoder endEncoding];
	}

	id<MTLCommandBuffer> commandBuffer = commandBuffers_[commandBufferIndex];
	[commandBuffer presentDrawable: drawable];
	[commandBuffer commit];
	encoders_.clear();
	commandBuffers_.clear();
}

@end


