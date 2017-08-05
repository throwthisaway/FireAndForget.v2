#import <Metal/Metal.h>

@interface Shaders : NSObject
- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat;
- (id<MTLRenderPipelineState> _Nonnull) selectPipeline: (size_t) index;
@end
