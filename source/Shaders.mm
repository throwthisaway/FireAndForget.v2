#import "Shaders.h"
#import <Foundation/Foundation.h>

@implementation Shaders
{
	id <MTLDevice> device_;
	id <MTLLibrary> library_;

	MTLPixelFormat pixelFormat_;
	id <MTLFunction> vert_, frag_;
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
	vert_ = [library_ newFunctionWithName:@"vertex_main"];
	frag_ = [library_ newFunctionWithName:@"fragment_main"];
}

- (void) setupPipeline: (MTLPixelFormat) pixelFormat {
	MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
	pipelineDescriptor.vertexFunction = vert_;
	pipelineDescriptor.fragmentFunction = frag_;
	pipelineDescriptor.colorAttachments[0].pixelFormat = pixelFormat;
	pipeline_ = [device_ newRenderPipelineStateWithDescriptor: pipelineDescriptor error: NULL];

	[pipelines_ addObject:pipeline_];
}

- (id<MTLRenderPipelineState>) selectPipeline: (size_t) index {
	return pipelines_[index];
}
@end
