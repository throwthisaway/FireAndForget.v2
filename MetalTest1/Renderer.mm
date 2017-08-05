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

void RendererWrapper::SubmitToPipeline(size_t index, uint8_t* uniforms, MeshDesc&& meshDesc) {
	[(__bridge Renderer*)self submitToPipeline:index withUniforms: uniforms withDesc: std::move(meshDesc)];
}

struct CmdDesc {
	MeshDesc mesh;
	uint8_t* uniforms;
};
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
	std::array<std::vector<CmdDesc>, Materials::Count> submitted_;
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

- (void) submitToPipeline: (size_t) index withUniforms: (uint8_t* _Nonnull) uniforms withDesc: (MeshDesc&&)meshDesc {
	submitted_[index].push_back({std::move(meshDesc), uniforms});
}

- (void) renderTo: (CAMetalLayer*) metalLayer {
	id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
	id<MTLTexture> texture = drawable.texture;

	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	passDescriptor.colorAttachments[0].texture = texture;
	passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(.5, .5, .5, 1.);

	id<MTLCommandBuffer> commandBuffer = [commandQueue_ commandBuffer];

	id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor: passDescriptor];
	for (size_t i = 0; i < submitted_.size(); ++i) {
		[commandEncoder setRenderPipelineState: [shaders_ selectPipeline:i]];
		for (const auto& cmd : submitted_[i]) {
			size_t attribIndex = 0;
			for (const auto& bufferIndex : cmd.mesh.buffers) {
				[commandEncoder setVertexBuffer: buffers_[bufferIndex].buffer offset: cmd.mesh.offset * buffers_[bufferIndex].elementSize atIndex: attribIndex];
				++attribIndex;
			}
			memcpy([buffers_[uniforms_[i]].buffer contents], cmd.uniforms, buffers_[uniforms_[i]].size);
			[commandEncoder setVertexBuffer: buffers_[uniforms_[i]].buffer offset: 0 atIndex: attribIndex];
			++attribIndex;

			[commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:cmd.mesh.offset vertexCount:cmd.mesh.count];
		}
	}
	[commandEncoder endEncoding];
	[commandBuffer presentDrawable: drawable];
	[commandBuffer commit];
	for (size_t i = 0; i< submitted_.size(); ++i) {
		submitted_[i].clear();
	}
}

@end


