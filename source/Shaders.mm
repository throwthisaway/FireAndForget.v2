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
		colorAttachmentFormats_[1] = MTLPixelFormatRGBA16Float;	// normalWS
		colorAttachmentFormats_[2] = MTLPixelFormatRGBA16Float;	// normalVS
		colorAttachmentFormats_[3] = MTLPixelFormatRGBA8Unorm;	// material
		colorAttachmentFormats_[4] = MTLPixelFormatRGBA32Float;	// debug
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
	pipelines_.resize(ShaderStructures::Count);
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
		pipelines_[ShaderStructures::Pos] = {pipeline, RenderPass::Geometry};
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
		pipelines_[ShaderStructures::Tex] = {pipeline, RenderPass::Geometry};
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
		pipelines_[ShaderStructures::Debug] = {pipeline, RenderPass::Geometry};
		if (error) NSLog(@"Debug %@", [error localizedDescription]);
	}

	{
		// Deferred
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"fsquad_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"deferred_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
		// debug...
		pipelineDescriptor.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA32Float;

		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_[ShaderStructures::Deferred] = {pipeline, RenderPass::Post};
		if (error) NSLog(@"Deferred %@", [error localizedDescription]);
	}

	{
		// DeferredPBR
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"fsquad_vs_main"];
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
		pipelines_[ShaderStructures::DeferredPBR] = {pipeline, RenderPass::Post};
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
		pipelines_[ShaderStructures::CubeEnvMap] = {pipeline, RenderPass::Pre};
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
		pipelines_[ShaderStructures::Bg] = {pipeline, RenderPass::Forward};
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
		pipelines_[ShaderStructures::Irradiance] = {pipeline, RenderPass::Pre};
		if (error) NSLog(@"Irradiance %@", [error localizedDescription]);
	}

	{
		// prefilter env
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
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"prefilter_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_[ShaderStructures::PrefilterEnv] = {pipeline, RenderPass::Pre};
		if (error) NSLog(@"Prefilter env %@", [error localizedDescription]);
	}

	{
		// BRDF lut
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"fsquad_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"integratebrdf_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRG16Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_[ShaderStructures::BRDFLUT] = {pipeline, RenderPass::Pre};
		if (error) NSLog(@"BRDF LUT%@", [error localizedDescription]);
	}

	{
		// Downsample
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"fsquad_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"downsample_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatR32Float;
		pipelineDescriptor.colorAttachments[0].blendingEnabled = NO;

		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_[ShaderStructures::Downsample] = {pipeline, RenderPass::Post};
		if (error) NSLog(@"Downsample %@", [error localizedDescription]);
	}

	{
		// ModoDNMR
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3; // pos
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.attributes[1].format = MTLVertexFormatFloat3; // normal
		vertexDesc.attributes[1].bufferIndex = 0;
		vertexDesc.attributes[1].offset = 3 * sizeof(float)/*pos*/;
		vertexDesc.attributes[2].format = MTLVertexFormatFloat3; // tangent
		vertexDesc.attributes[2].bufferIndex = 0;
		vertexDesc.attributes[2].offset = 3 * sizeof(float)/*pos*/ + 3 * sizeof(float)/*nomal*/;
		vertexDesc.attributes[3].format = MTLVertexFormatFloat2; // uv
		vertexDesc.attributes[3].bufferIndex = 0;
		vertexDesc.attributes[3].offset = 3 * sizeof(float)/*pos*/ + 3 * sizeof(float)/*nomal*/ + 3 * sizeof(float)/*tangent*/;
		vertexDesc.layouts[0].stride = 3 * sizeof(float) /*pos*/+ 3 * sizeof(float)/*normal*/ + 3 * sizeof(float)/*tangent*/ + 2 * sizeof(float) /*uv*/;
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"modo_dn_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"modo_dnmr_fs_main"];
		for (int i = 0; i < RenderTargetCount; ++i)
			pipelineDescriptor.colorAttachments[i].pixelFormat = colorAttachmentFormats_[i];
		pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_[ShaderStructures::ModoDNMR] = {pipeline, RenderPass::Geometry};
		if (error) NSLog(@"ModoDNMR %@", [error localizedDescription]);
	}

	{
		// ModoDN
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		MTLVertexDescriptor* vertexDesc = [MTLVertexDescriptor new];
		vertexDesc.attributes[0].format = MTLVertexFormatFloat3; // pos
		vertexDesc.attributes[0].bufferIndex = 0;
		vertexDesc.attributes[0].offset = 0;
		vertexDesc.attributes[1].format = MTLVertexFormatFloat3; // normal
		vertexDesc.attributes[1].bufferIndex = 0;
		vertexDesc.attributes[1].offset = 3 * sizeof(float)/*pos*/;
		vertexDesc.attributes[2].format = MTLVertexFormatFloat3; // tangent
		vertexDesc.attributes[2].bufferIndex = 0;
		vertexDesc.attributes[2].offset = 3 * sizeof(float)/*pos*/ + 3 * sizeof(float)/*nomal*/;
		vertexDesc.attributes[3].format = MTLVertexFormatFloat2; // uv
		vertexDesc.attributes[3].bufferIndex = 0;
		vertexDesc.attributes[3].offset = 3 * sizeof(float)/*pos*/ + 3 * sizeof(float)/*nomal*/ + 3 * sizeof(float)/*tangent*/;
		vertexDesc.layouts[0].stride = 3 * sizeof(float) /*pos*/+ 3 * sizeof(float)/*normal*/ + 3 * sizeof(float)/*tangent*/ + 2 * sizeof(float) /*uv*/;
		vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		pipelineDescriptor.vertexDescriptor = vertexDesc;
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"modo_dn_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"modo_dn_fs_main"];
		for (int i = 0; i < RenderTargetCount; ++i)
			pipelineDescriptor.colorAttachments[i].pixelFormat = colorAttachmentFormats_[i];
		pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_[ShaderStructures::ModoDN] = {pipeline, RenderPass::Geometry};
		if (error) NSLog(@"ModoDN %@", [error localizedDescription]);
	}

	{
		// SSAO
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"fsquad_viewpos_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"ssao_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatR32Float;
		pipelineDescriptor.colorAttachments[0].blendingEnabled = NO;

		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_[ShaderStructures::SSAOShader] = {pipeline, RenderPass::Post};
		if (error) NSLog(@"SSAO %@", [error localizedDescription]);
	}

	{
		// Blur4x4R32
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
		pipelineDescriptor.vertexFunction = [library_ newFunctionWithName:@"fsquad_vs_main"];
		pipelineDescriptor.fragmentFunction = [library_ newFunctionWithName:@"blur4x4r32_fs_main"];
		pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatR32Float;
		pipelineDescriptor.colorAttachments[0].blendingEnabled = NO;

		id <MTLRenderPipelineState> pipeline = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: &error];
		pipelines_[ShaderStructures::Blur4x4R32] = {pipeline, RenderPass::Post};
		if (error) NSLog(@"Blur4x4R32 %@", [error localizedDescription]);
	}
}

- (const PipelineState&) selectPipeline: (size_t) index {
	return pipelines_[index];
}

- (size_t) getPipelineCount {
	return pipelines_.size();
}
@end
