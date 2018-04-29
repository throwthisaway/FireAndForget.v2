#import "Shaders.h"
#import <Foundation/Foundation.h>
#include "cpp/ShaderStructures.h"
using namespace ShaderStructures;

@implementation Shaders
{
	id <MTLDevice> device_;
	id <MTLLibrary> library_;

	MTLPixelFormat pixelFormat_;

	std::vector<PipelineState> pipelines_;

	MTLPixelFormat colorAttachmentFormats_[RenderTargetCount];
}
- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat {
	if (self = [super init]) {
		device_ = device;
		pixelFormat_ = pixelFormat;
		colorAttachmentFormats_[0] = MTLPixelFormatRGBA8Unorm;	// color
		colorAttachmentFormats_[1] = MTLPixelFormatRG16Unorm;	// normal
		colorAttachmentFormats_[2] = MTLPixelFormatRGBA8Unorm;	// material
		colorAttachmentFormats_[3] = MTLPixelFormatRGBA32Float;	// debug
		[self setupShaders];
		[self setupPipeline: pixelFormat];
	}
	return self;
}

- (MTLPixelFormat*) getColorAttachmentFormats {
	return colorAttachmentFormats_;
}

- (void) setupShaders {
	library_ = [device_ newDefaultLibrary];
}

- (void) setupPipeline: (MTLPixelFormat) pixelFormat {
	NSError* error = nullptr;
	{
		// Pos
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.attributes[1].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[1].bufferIndex = 1;
		vertexDesc.attributes[1].offset = 0;
		vertexDesc.layouts[0].stride = 3 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		vertexDesc.layouts[1].stride = 3 * sizeof(float);
		vertexDesc.layouts[1].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"pos_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"pos_fs_main"];
		for (int i = 0; i < RenderTargetCount; ++i)
			pipelineDescriptor.colorAttachments[i].pixelFormat = colorAttachmentFormats_[i];
		pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, false});
		if (error) NSLog(@"Pos: %@", [error localizedDescription]);
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
		for (int i = 0; i < RenderTargetCount; ++i)
			pipelineDescriptor.colorAttachments[i].pixelFormat = colorAttachmentFormats_[i];
		pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, false});
		if (error) NSLog(@"Tex %@", [error localizedDescription]);
	}

	{
		// Debug
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.layouts[0].stride = 3 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"debug_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"debug_fs_main"];
		for (int i = 0; i < RenderTargetCount; ++i)
			pipelineDescriptor.colorAttachments[i].pixelFormat = colorAttachmentFormats_[i];
		pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, false});
		if (error) NSLog(@"Debug %@", [error localizedDescription]);
	}

	{
		// Deferred
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat2;
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.attributes[1].format = MTLVertexFormatFloat2;
		vertexDesc.attributes[1].bufferIndex = 0;
		vertexDesc.attributes[1].offset = 2 * sizeof(float);
		vertexDesc.layouts[0].stride = 4 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"deferred_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"deferred_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, true});
		if (error) NSLog(@"%s", [error localizedDescription]);
	}
}

- (const PipelineState&) selectPipeline: (size_t) index {
	return pipelines_[index];
}

- (size_t) getPipelineCount {
	return pipelines_.size();
}
@end
