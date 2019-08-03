#import <Metal/Metal.h>

enum class RenderPass {Pre, Forward, Geometry, Post};
struct PipelineState {
	id<MTLRenderPipelineState> _Nonnull pipeline;
	RenderPass renderPass;
};

@interface Shaders : NSObject
- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat;
- (const PipelineState&) selectPipeline: (size_t) index;
- (size_t) getPipelineCount;
- (MTLPixelFormat* _Nonnull) getColorAttachmentFormats;
@end
