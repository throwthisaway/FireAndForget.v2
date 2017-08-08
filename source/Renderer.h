#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
struct MeshDesc;

@interface Renderer : NSObject 
- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat;
- (size_t) createBuffer: (const void* _Nonnull) buffer withLength: (size_t) length withElementSize: (size_t) elementSize;
- (size_t) beginRender;
- (size_t) startRenderPass:  (id<MTLTexture> _Nonnull) texture withCommandBuffer: (size_t) commandBufferIndex ;
- (void) renderTo: (id<CAMetalDrawable> _Nonnull) drawable withCommandBuffer: (size_t) commandBufferIndex ;
- (void) submitToEncoder: (size_t) encoderIndex withPipeline: (size_t) pipelineIndex withUniforms: (uint8_t* _Nonnull) uniforms withDesc: (MeshDesc&&)meshDesc;
@end
