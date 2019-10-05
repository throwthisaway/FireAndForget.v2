#import "ViewController.h"
#import "MetalView.h"
#import <Metal/Metal.h>
#import "Renderer.h"
#include "cpp/Scene.hpp"
#include "cpp/Timer.hpp"
#include <Carbon/Carbon.h>
#include "UI.h"

@implementation ViewController
{
	id <MTLDevice> device;

	MTLPixelFormat pixelformat_;
	MetalView* metalView;
	Renderer renderer_;
	Timer timer_;
	Scene scene_;
	bool keys[512];
}
- (void)viewDidLoad {
	[super viewDidLoad];
	pixelformat_ = MTLPixelFormatBGRA8Unorm;
	[self setupLayer];
}
-(void)setupLayer {
	device = MTLCreateSystemDefaultDevice();
	metalView = (MetalView*)self.view;
	metalView.delegate = self;
	CAMetalLayer* metalLayer = [metalView getMetalLayer];

	metalLayer.device = device;
	metalLayer.pixelFormat = pixelformat_;
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
- (void)mouseDown:(NSEvent *)event {
	scene_.input.Start(event.locationInWindow.x, -event.locationInWindow.y);
}
- (void)rightMouseDown:(NSEvent *)event {
	scene_.input.Start(event.locationInWindow.x, -event.locationInWindow.y);
}
- (void)mouseMoved:(NSEvent *)event {
	NSLog(@"mouse moved %ld", (long)event.buttonNumber);
	// TODO:: not fired
}
- (void)mouseDragged:(NSEvent *)event {
	//NSLog(@"mouse dragged %f %f", event.locationInWindow.x, event.locationInWindow.y);

	if (event.modifierFlags & NSEventModifierFlagCommand)
		scene_.input.TranslateXZ(event.locationInWindow.x, -event.locationInWindow.y);
	else
		scene_.input.Rotate(event.locationInWindow.x, -event.locationInWindow.y);
	if (event.modifierFlags & NSEventModifierFlagControl)
		scene_.UpdateSceneTransform();
	else
		scene_.UpdateCameraTransform();
}
- (void)rightMouseDragged:(NSEvent *)event {
	//NSLog(@"mouse dragged %f %f", event.locationInWindow.x, event.locationInWindow.y);
	scene_.input.TranslateY(-event.locationInWindow.y);
}
- (void)keyDown:(NSEvent *)event {
	if (event.keyCode == kVK_Escape) {
		[[[self view] window] close];
		return;
	}
	assert(event.keyCode < sizeof(keys) / sizeof(keys[0]));
	keys[event.keyCode] = true;
}
- (void)keyUp:(NSEvent *) event {
	assert(event.keyCode < sizeof(keys) / sizeof(keys[0]));
	keys[event.keyCode] = false;
}

@end
