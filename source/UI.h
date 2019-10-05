#pragma once
#import <Metal/Metal.h>
namespace ui {
bool Init(id<MTLDevice> device);
id<MTLCommandBuffer> Render(id<MTLTexture> tex, id<MTLCommandBuffer> commandBuffer);
void Update(double frame, double total);
void Shutdown();
void OnResize(int width, int height);

bool UpdateMousePos(int x, int y);
bool UpdateMouseButton(bool l, bool r, bool m);
bool UpdateMouseWheel(int delta, bool horizontal);
bool UpdateKeyboard(int key, bool down);
bool UpdateKeyboardInput(int key);
void UpdateKeyboardModifiers(bool ctrl, bool alt, bool shift);
//bool UpdateMouseCursor(Windows::UI::Core::CoreWindow^ window);
}
