
#import "MetalView.h"
#import <Metal/Metal.h>
#import <CoreVideo/CVDisplayLink.h>

@implementation MetalView {
	CVDisplayLinkRef displayLink;
	NSTrackingRectTag trackingRect_;
}

@synthesize delegate;

CVReturn displayCallback(CVDisplayLinkRef displayLink,
						 const CVTimeStamp *inNow,
						 const CVTimeStamp *inOutputTime,
						 CVOptionFlags flagsIn,
						 CVOptionFlags *flagsOut,
						 void *displayLinkContext)
{
	MetalView *view = (__bridge MetalView*)displayLinkContext;
	[view renderForTime: *inOutputTime];
	return kCVReturnSuccess;
}

- (CAMetalLayer*) getMetalLayer {
	return (CAMetalLayer*)[self layer];
}

- (CALayer*)makeBackingLayer {
	return [[CAMetalLayer alloc] init];
}

-(void)registerDisplayLink {
	CGDirectDisplayID displayID = CGMainDisplayID();
	CVReturn error = CVDisplayLinkCreateWithCGDisplay(displayID, &displayLink);
	NSAssert((kCVReturnSuccess == error), @"Creating Display Link error %d", error);

	error = CVDisplayLinkSetOutputCallback(displayLink, displayCallback, (__bridge void*)self);
	NSAssert((kCVReturnSuccess == error), @"Setting Display Link callback error %d", error);
	CVDisplayLinkStart(displayLink);
}
-(void)render {
	[delegate render];
}
-(void)renderForTime: (CVTimeStamp)time {
	// TODO:: causes choppy framerate,  but main thread checker doesn't complain
//	dispatch_async(dispatch_get_main_queue(), ^{
//		//[self setNeedsDisplay:YES];
//		[delegate render];
//	});

	// TODO:: this should work:
//	-(void) engineUpdate {
//		GetEngine().Frame();
//	}
//
//	-(void)renderForTime: (CVTimeStamp)time {
//		//[self engineUpdate];
//		[self performSelectorOnMainThread: @selector(engineUpdate) withObject: nil waitUntilDone: TRUE];
//	}


	//[delegate render];
	[self performSelectorOnMainThread: @selector(render) withObject: nil waitUntilDone: TRUE];

}
- (BOOL) wantsLayer {
	return YES;
}

- (BOOL)wantsUpdateLayer {
	return YES;
}

//- (void) displayLayer:(CALayer *)layer {
//	[delegate render];
//}

- (nullable instancetype)initWithCoder:(NSCoder *)aDecoder {
	if (self = [super initWithCoder:aDecoder]) {
		//self.wantsLayer = TRUE;
		//self.layerContentsRedrawPolicy =  NSViewLayerContentsRedrawOnSetNeedsDisplay;
		self.layerContentsRedrawPolicy =  NSViewLayerContentsRedrawNever;
	}
	return self;
}

//- (void)displayLayer:(CALayer*)layer {
//	[delegate render];
//}

- (void)viewDidMoveToSuperview {
	[super viewDidMoveToSuperview];

	if (self.superview) {
		[self registerDisplayLink];
	} else {
		displayLink = nil;
	}
}

-(void)windowWillClose:(NSNotification*)note {
	[self removeTrackingRect: trackingRect_];
	CVDisplayLinkStop(displayLink);
	CVDisplayLinkRelease(displayLink);
	[[NSApplication sharedApplication] terminate:self];
}

- (void)viewDidMoveToWindow {
	trackingRect_ = [self addTrackingRect:[self bounds] owner:self userData:NULL assumeInside:NO];
}

- (BOOL)acceptsFirstResponder {
	// keyevents handled in ViewController
	return YES;
}

@end
