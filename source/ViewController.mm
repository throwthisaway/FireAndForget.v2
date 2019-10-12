#import "ViewController.h"
#import "MetalView.h"
#import <Metal/Metal.h>
#import "Renderer.h"
#include "cpp/Scene.hpp"
#include "cpp/Timer.hpp"
#include <Carbon/Carbon.h>
#include "UI.h"
#include "cpp/DebugUI.h"

@implementation ViewController
{
	id <MTLDevice> device;

	MTLPixelFormat pixelformat_;
	MetalView* metalView;
	Renderer renderer_;
	Timer timer_;
	Scene scene_;
	bool keys[512];
	bool l,r;
	float contentScale;
}
- (void)viewDidLoad {
	[super viewDidLoad];
	pixelformat_ = MTLPixelFormatBGRA8Unorm;
	l = r = false;
	[self setupLayer];
}
-(void)setupLayer {
	device = MTLCreateSystemDefaultDevice();
	metalView = (MetalView*)self.view;
	metalView.delegate = self;
	CAMetalLayer* metalLayer = [metalView getMetalLayer];

	metalLayer.device = device;
	metalLayer.pixelFormat = pixelformat_;
	NSScreen* screen = [NSScreen mainScreen];
	contentScale = metalLayer.contentsScale = [screen backingScaleFactor];
	renderer_.Init(device, pixelformat_);
	renderer_.OnResize( metalLayer.bounds.size.width, metalLayer.bounds.size.height);
	scene_.Init(&renderer_, metalLayer.bounds.size.width, metalLayer.bounds.size.height);
	////[metalLayer setNeedsDisplay];
	//commandQueue = [[metalView getMetalLayer].device newCommandQueue];
}

- (void) render {
	timer_.Tick();
	scene_.Update(timer_.FrameMs(), timer_.TotalMs());
	@autoreleasepool {
		// TODO:: triple buffering!!!
		id<CAMetalDrawable> drawable = [[metalView getMetalLayer] nextDrawable];
		renderer_.SetDrawable(drawable);
		scene_.Render();
	}
}
- (void)viewWillTransitionToSize:(NSSize)newSize {
	renderer_.OnResize((int)newSize.width, (int)newSize.height);
}

- (void)setRepresentedObject:(id)representedObject {
	[super setRepresentedObject:representedObject];

	// Update the view, if already loaded.
}

- (void)mouseEntered:(NSEvent *)event {
	//NSLog(@"mouse entered");
}

- (void)mouseExited:(NSEvent *)event {
	//NSLog(@"mouse exited");
}
- (void) mouseUp:(NSEvent *)event {
	l = false;
	DEBUGUI_PROCESSINPUT(ui::UpdateMouseButton(l, r, false));
}
- (void)mouseDown:(NSEvent *)event {
	l = true;
	DEBUGUI(ui::UpdateKeyboardModifiers((event.modifierFlags & NSEventModifierFlagControl) == NSEventModifierFlagControl, (event.modifierFlags & NSEventModifierFlagOption) == NSEventModifierFlagOption, (event.modifierFlags & NSEventModifierFlagShift) == NSEventModifierFlagShift));
	if (!DEBUGUI_PROCESSINPUT(ui::UpdateMouseButton(l, r, false))) {
		NSPoint pos = [event locationInWindow];
		NSPoint local_point = [metalView convertPoint:pos fromView:nil];
		local_point.y = metalView.bounds.size.height - local_point.y;
		scene_.input.Start(local_point.x * contentScale, local_point.y * contentScale);
	}
}
- (void)rightMouseUp:(NSEvent *)event {
	r = false;
	DEBUGUI_PROCESSINPUT(ui::UpdateMouseButton(l, r, false));
}
- (void)rightMouseDown:(NSEvent *)event {
	r = true;
	DEBUGUI(ui::UpdateKeyboardModifiers((event.modifierFlags & NSEventModifierFlagControl) == NSEventModifierFlagControl, (event.modifierFlags & NSEventModifierFlagOption) == NSEventModifierFlagOption, (event.modifierFlags & NSEventModifierFlagShift) == NSEventModifierFlagShift));
	if (!DEBUGUI_PROCESSINPUT(ui::UpdateMouseButton(l, r, false))) {
		NSPoint pos = [event locationInWindow];
		NSPoint local_point = [metalView convertPoint:pos fromView:nil];
		local_point.y = metalView.bounds.size.height - local_point.y;
		scene_.input.Start(local_point.x * contentScale, local_point.y * contentScale);
	}
}
- (void)mouseMoved:(NSEvent *)event {
	NSPoint pos = [event locationInWindow];
	NSPoint local_point = [metalView convertPoint:pos fromView:nil];
	local_point.y = metalView.bounds.size.height - local_point.y;
	if (DEBUGUI_PROCESSINPUT(ui::UpdateMousePos(local_point.x * contentScale, local_point.y * contentScale))) {
	}
}
- (void)mouseDragged:(NSEvent *)event {
	//NSLog(@"mouse dragged %f %f", event.locationInWindow.x, event.locationInWindow.y);
	NSPoint pos = [event locationInWindow];
	NSPoint local_point = [metalView convertPoint:pos fromView:nil];
	local_point.y = metalView.bounds.size.height - local_point.y;
	if (DEBUGUI_PROCESSINPUT(ui::UpdateMousePos(local_point.x * contentScale, local_point.y * contentScale))) {
		ui::UpdateKeyboardModifiers((event.modifierFlags & NSEventModifierFlagControl) == NSEventModifierFlagControl, (event.modifierFlags & NSEventModifierFlagOption) == NSEventModifierFlagOption, (event.modifierFlags & NSEventModifierFlagShift) == NSEventModifierFlagShift);
		ui::UpdateMouseButton(true, false, false);
		return;
	}
	if (event.modifierFlags & NSEventModifierFlagCommand)
		scene_.input.TranslateXZ(local_point.x * contentScale, local_point.y * contentScale);
	else
		scene_.input.Rotate(local_point.x * contentScale, local_point.y * contentScale);
	if (event.modifierFlags & NSEventModifierFlagControl)
		scene_.UpdateSceneTransform();
	else
		scene_.UpdateCameraTransform();
}
- (void)rightMouseDragged:(NSEvent *)event {
	//NSLog(@"mouse dragged %f %f", event.locationInWindow.x, event.locationInWindow.y);
	NSPoint pos = [event locationInWindow];
	NSPoint local_point = [metalView convertPoint:pos fromView:nil];
	local_point.y = metalView.bounds.size.height - local_point.y;
	if (DEBUGUI_PROCESSINPUT(ui::UpdateMousePos(local_point.x * contentScale, local_point.y * contentScale))) {
		ui::UpdateKeyboardModifiers((event.modifierFlags & NSEventModifierFlagControl) == NSEventModifierFlagControl, (event.modifierFlags & NSEventModifierFlagOption) == NSEventModifierFlagOption, (event.modifierFlags & NSEventModifierFlagShift) == NSEventModifierFlagShift);
		ui::UpdateMouseButton(false, true, false);
		return;
	}
	scene_.input.TranslateY(local_point.y);
	if (event.modifierFlags & NSEventModifierFlagControl)
		scene_.UpdateSceneTransform();
	else
		scene_.UpdateCameraTransform();
}

- (void)keyDown:(NSEvent *)event {
	if (event.keyCode == kVK_Escape) {
		[[[self view] window] close];
		return;
	}
	DEBUGUI(ui::UpdateKeyboardModifiers((event.modifierFlags & NSEventModifierFlagControl) == NSEventModifierFlagControl, (event.modifierFlags & NSEventModifierFlagOption) == NSEventModifierFlagOption, (event.modifierFlags & NSEventModifierFlagShift) == NSEventModifierFlagShift));
	NSString* chars = [event characters];
	auto ch = [chars characterAtIndex:0] ;
	DEBUGUI_PROCESSINPUT(ui::UpdateKeyboardInput(ch));
	if (!DEBUGUI_PROCESSINPUT(ui::UpdateKeyboard(ch, true))) {
		assert(ch < sizeof(keys) / sizeof(keys[0]));
		keys[ch] = true;
	}
}
- (void)keyUp:(NSEvent *) event {
	DEBUGUI(ui::UpdateKeyboardModifiers((event.modifierFlags & NSEventModifierFlagControl) == NSEventModifierFlagControl, (event.modifierFlags & NSEventModifierFlagOption) == NSEventModifierFlagOption, (event.modifierFlags & NSEventModifierFlagShift) == NSEventModifierFlagShift));
	NSString* chars = [event characters];
	auto ch = [chars characterAtIndex:0];
	if (!DEBUGUI_PROCESSINPUT(ui::UpdateKeyboard(ch, false))) {
		assert(ch < sizeof(keys) / sizeof(keys[0]));
		keys[ch] = false;
	}
}
- (void)scrollWheel:(NSEvent *)event {
	if (!DEBUGUI_PROCESSINPUT(ui::UpdateMouseWheel(event.deltaY, false)) && !DEBUGUI_PROCESSINPUT(ui::UpdateMouseWheel(event.deltaX, true))) {
		
	}
}
- (void)cursorUpdate:(NSEvent *)event {
//   switch(type) {
//		case prezi::platform::CursorType::kArrow:
//			[[NSCursor arrowCursor] set];
//			break;
//		case prezi::platform::CursorType::kPointingHand:
//			[[NSCursor pointingHandCursor] set];
//			break;
//	}
}
@end
