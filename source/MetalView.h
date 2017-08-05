#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import "MetalViewDelegate.h"

@interface MetalView : NSView
@property (readonly, getter=getMetalLayer) CAMetalLayer* metalLayer;
@property (weak) id<MetalViewDelegate> delegate;
@end
