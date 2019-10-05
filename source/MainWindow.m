#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

@interface MainWindow : NSWindow
@end
@implementation MainWindow

-(BOOL)acceptsMouseMovedEvents {
	return YES;
}

@end
