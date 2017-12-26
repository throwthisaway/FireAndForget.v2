#import "Shaders.h"
#import <Foundation/Foundation.h>

@implementation Shaders
{
	id <MTLDevice> device_;
	id <MTLLibrary> library_;

	MTLPixelFormat pixelFormat_;
	id <MTLRenderPipelineState> pipeline_;

	NSMutableArray<id<MTLRenderPipelineState>>* pipelines_;
}
- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat {
	if (self = [super init]) {
		device_ = device;
		pixelFormat_ = pixelFormat;
		pipelines_ = [[NSMutableArray alloc]init];
		[self setupShaders];
		[self setupPipeline: pixelFormat];
	}
	return self;
}
- (void) setupShaders {
	library_ = [device_ newDefaultLibrary];
}

- (void) setupPipeline: (MTLPixelFormat) pixelFormat {
	{
		// ColPos or appropriate default shader to be removed later
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"vertex_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"fragment_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
		pipeline_ = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: NULL];
		[pipelines_ addObject:pipeline_];
	}

	{
		// Pos
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.layouts[0].stride = 3 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"pos_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"pos_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
		pipeline_ = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: NULL];
		[pipelines_ addObject:pipeline_];
	}

	{
		// Tex
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.attributes[1].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[1].bufferIndex = 1;
		vertexDesc.attributes[1].offset = 0;
		vertexDesc.attributes[2].format = MTLVertexFormatFloat2;
		vertexDesc.attributes[2].bufferIndex = 2;
		vertexDesc.attributes[2].offset = 0;
		vertexDesc.layouts[0].stride = 3 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		vertexDesc.layouts[1].stride = 3 * sizeof(float);
		vertexDesc.layouts[1].stepFunction = MTLVertexStepFunctionPerVertex;
		vertexDesc.layouts[2].stride = 2 * sizeof(float);
		vertexDesc.layouts[2].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"tex_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"tex_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
		pipeline_ = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: NULL];
		[pipelines_ addObject:pipeline_];
	}
}

- (id<MTLRenderPipelineState>) selectPipeline: (size_t) index {
	return pipelines_[index];
}

- (size_t) getPipelineCount {
	return [pipelines_ count];
}
@end
