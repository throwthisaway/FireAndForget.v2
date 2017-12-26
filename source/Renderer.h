#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <vector>
#include "cpp/RendererTypes.h"
struct Mesh;

@interface Renderer : NSObject
- (nullable instancetype) initWithDevice: (id<MTLDevice> _Nonnull) device andPixelFormat: (MTLPixelFormat) pixelFormat;
- (BufferIndex) createBuffer: (const void* _Nonnull) buffer withLength: (size_t) length withElementSize: (size_t) elementSize;
- (void) beginRender;
- (void) startRenderPass: (id<MTLTexture> _Nonnull) texture;
- (void) renderTo: (id<CAMetalDrawable> _Nonnull) drawable;
- (void) submitToEncoder: (size_t) encoderIndex withPipeline: (size_t) pipelineIndex vsUniforms: (const std::vector<size_t>&) vsBuffers fsUniforms: (const std::vector<size_t>&) fsBuffers withMesh: (const Mesh&)mesh;
- (uint32_t) getCurrentFrameIndex;

- (ResourceHeapHandle) getStaticShaderResourceHeap;
- (ShaderResourceIndex) getShaderResourceIndexFromHeap: (ResourceHeapHandle)shaderResourceHeap withSize: (size_t)size andCount: (uint16_t) count;
- (void) updateShaderResource: (ShaderResourceIndex)shaderResourceIndex withData: (const void* _Null_unspecified)data andSize: (size_t)size;
- (ShaderResourceIndex) getShaderResourceIndexFromHeap: (ResourceHeapHandle)shaderResourceHeap withTextureIndex: (BufferIndex)textureIndex;
@end
