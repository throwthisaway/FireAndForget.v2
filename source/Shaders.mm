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
		vertexDesc.attributes[1].bufferIndex = 0;
		vertexDesc.attributes[1].offset = 3 * sizeof(float);
		vertexDesc.layouts[0].stride = 3 * sizeof(float) + 3 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"pos_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"pos_fs_main"];
		for (int i = 0; i < RenderTargetCount; ++i)
			pipelineDescriptor.colorAttachments[i].pixelFormat = colorAttachmentFormats_[i];
		pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, RenderPass::Deferred});
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
		vertexDesc.attributes[1].bufferIndex = 0;
		vertexDesc.attributes[1].offset = 3 * sizeof(float);
		vertexDesc.attributes[2].format = MTLVertexFormatFloat2;
		vertexDesc.attributes[2].bufferIndex = 0;
		vertexDesc.attributes[2].offset = 3 * sizeof(float) + 3 * sizeof(float);
		vertexDesc.layouts[0].stride = 3 * sizeof(float) + 3 * sizeof(float) + 2 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"tex_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"tex_fs_main"];
		for (int i = 0; i < RenderTargetCount; ++i)
			pipelineDescriptor.colorAttachments[i].pixelFormat = colorAttachmentFormats_[i];
		pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, RenderPass::Deferred});
		if (error) NSLog(@"Tex %@", [error localizedDescription]);
	}

	{
		// Debug
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.attributes[1].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[1].bufferIndex = 0;
		vertexDesc.attributes[1].offset = 3 * sizeof(float);
		vertexDesc.layouts[0].stride = 3 * sizeof(float) + 3 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"debug_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"debug_fs_main"];
		for (int i = 0; i < RenderTargetCount; ++i)
			pipelineDescriptor.colorAttachments[i].pixelFormat = colorAttachmentFormats_[i];
		pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, RenderPass::Deferred});
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
		// debug...
		pipelineDescriptor.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA32Float;

		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, RenderPass::Post});
		if (error) NSLog(@"Deferred %@", [error localizedDescription]);
	}

	{
		// DeferredPBR
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
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"deferred_pbr_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"deferred_pbr_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
		pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
		pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
		pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
		pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
		pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
		pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		// debug...
		pipelineDescriptor.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA32Float;

		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, RenderPass::Post});
		if (error) NSLog(@"DeferredPBR %@", [error localizedDescription]);
	}

	{
		// CubeEnvMap
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.attributes[1].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[1].bufferIndex = 0;
		vertexDesc.attributes[1].offset = 3 * sizeof(float);
		vertexDesc.layouts[0].stride = 3 * sizeof(float) + 3 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"cubeenv_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"cubeenv_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, RenderPass::Pre});
		if (error) NSLog(@"CubeEnvMap %@", [error localizedDescription]);
	}

	{
		// Bg
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.attributes[1].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[1].bufferIndex = 0;
		vertexDesc.attributes[1].offset = 3 * sizeof(float);
		vertexDesc.layouts[0].stride = 3 * sizeof(float) + 3 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"bg_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"bg_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
		//pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, RenderPass::Forward});
		if (error) NSLog(@"Bg %@", [error localizedDescription]);
	}

	{
		// Ir
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.attributes[1].format = MTLVertexFormatFloat3;
		vertexDesc.attributes[1].bufferIndex = 0;
		vertexDesc.attributes[1].offset = 3 * sizeof(float);
		vertexDesc.layouts[0].stride = 3 * sizeof(float) + 3 * sizeof(float);
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"cubeenv_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"cubeir_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_.push_back({pipeline, RenderPass::Pre});
		if (error) NSLog(@"Irradiance %@", [error localizedDescription]);
	}
}

- (const PipelineState&) selectPipeline: (size_t) index {
	return pipelines_[index];
}

- (size_t) getPipelineCount {
	return pipelines_.size();
}
@end
