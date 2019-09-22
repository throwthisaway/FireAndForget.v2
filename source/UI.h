#pragma once
#import <Metal/Metal.h>
namespace ui {
bool Init(id<MTLDevice> device);
id<MTLCommandBuffer> Render(id<MTLTexture> tex, id<MTLCommandBuffer> commandBuffer);
void Update(double frame, double total);
void Shutdown();
void OnResize(int width, int height);
}
