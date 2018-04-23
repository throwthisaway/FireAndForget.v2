#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "cpp/RendererTypes.h"
#include "cpp/ShaderStructures.h"
struct Mesh;

@interface Renderer : NSObject
- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat;
- (BufferIndex) createBuffer: (const void* _Nonnull) buffer withLength: (size_t) length withElementSize: (size_t) elementSize;
- (TextureIndex) createTexture: (const void* _Nonnull) buffer withWidth: (uint64_t) width withHeight: (uint32_t) height withBytesPerPixel: (uint8_t) bytesPerPixel withPixelFormat: (MTLPixelFormat) format withLabel: (NSString* _Nullable) label;
- (void) beginRender;
- (void) startRenderPass: (id<MTLTexture> _Nonnull) texture;
- (void) renderTo: (id<CAMetalDrawable> _Nonnull) drawable;
- (ShaderResourceIndex) createShaderResource: (uint32_t) length withCount: (uint16_t) count;
- (void) updateShaderResource: (ShaderResourceIndex)shaderResourceIndex withData: (const void* _Null_unspecified)data andSize: (size_t)size;
- (uint32_t) getCurrentFrameIndex;

-(void) submitDebugCmd: (const ShaderStructures::DebugCmd&) cmd;
-(void) submitPosCmd: (const ShaderStructures::PosCmd&) cmd;
-(void) submitTexCmd: (const ShaderStructures::TexCmd&) cmd;
@end
