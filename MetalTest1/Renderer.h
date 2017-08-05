#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
struct MeshDesc;

@interface Renderer : NSObject 
- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat;
- (size_t) createBuffer: (const void* _Nonnull) buffer withLength: (size_t) length withElementSize: (size_t) elementSize;
- (void) renderTo: (CAMetalLayer* _Nullable) metalLayer;
- (void) submitToPipeline: (size_t) index withUniforms: (uint8_t* _Nonnull) unfiorms withDesc: (MeshDesc&&)meshDesc;
@end
