#import "ViewController.h"
#import "MetalView.h"
#import <Metal/Metal.h>
#import "Renderer.h"
#import "cpp/RendererWrapper.h"
#include "cpp/Scene.hpp"
#include "cpp/Timer.hpp"
#include <Carbon/Carbon.h>

@implementation ViewController
{
	id <MTLDevice> device;
	//id <MTLBuffer> vertices, colors;
	//id <MTLLibrary> library;
	//id <MTLFunction> vert, frag;
	//id <MTLRenderPipelineState> pipeline;
	//id <MTLCommandQueue> commandQueue;

	MTLPixelFormat pixelformat_;
	MetalView* metalView;
	Renderer* renderer_;
	RendererWrapper rendererWrapper_;
	Timer timer_;
	Scene scene_;
	bool keys[512];
}
- (void)viewDidLoad {
	[super viewDidLoad];
	pixelformat_ = MTLPixelFormatBGRA8Unorm;
	[self setupLayer];
	//[self setupVBO];
	//[self setupShader];
	//[self setupPipeline];
	renderer_ = [[Renderer alloc] initWithDevice: device andPixelFormat: pixelformat_];
	rendererWrapper_.Init((__bridge void*)renderer_);
	scene_.Init(&rendererWrapper_);
}
-(void)setupLayer {
	device = MTLCreateSystemDefaultDevice();
	metalView = (MetalView*)self.view;
	metalView.delegate = self;
	CAMetalLayer* metalLayer = [metalView getMetalLayer];

	metalLayer.device = device;
	metalLayer.pixelFormat = pixelformat_;
	////[metalLayer setNeedsDisplay];
	//commandQueue = [[metalView getMetalLayer].device newCommandQueue];
}

-(void)setupVBO {
	static const float positions[] =
	{
		0.0,  0.5, 0, 1,
		-0.5, -0.5, 0, 1,
		0.5, -0.5, 0, 1,
	};

	static const float col[] =
	{
		1, 0, 0, 1,
		0, 1, 0, 1,
		0, 0, 1, 1,
	};

	//vertices = [device newBufferWithBytes:positions length:sizeof(positions) options: MTLResourceOptionCPUCacheModeDefault];
	//colors = [device newBufferWithBytes:col length:sizeof(col) options:MTLResourceOptionCPUCacheModeDefault];
}

-(void)setupShader {
	//library = [device newDefaultLibrary];
	//vert = [library newFunctionWithName:@"vertex_main"];
	//frag = [library newFunctionWithName:@"fragment_main"];
}

-(void)setupPipeline {
	//MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];
	//pipelineDescriptor.vertexFunction = vert;
	//pipelineDescriptor.fragmentFunction = frag;
	//pipelineDescriptor.colorAttachments[0].pixelFormat = [metalView getMetalLayer].pixelFormat;
	//pipeline = [device newRenderPipelineStateWithDescriptor: pipelineDescriptor error: NULL];
}

- (void) render {
	/*id<CAMetalDrawable> drawable = [[metalView getMetalLayer] nextDrawable];
	id<MTLTexture> texture = drawable.texture;

	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	passDescriptor.colorAttachments[0].texture = texture;
	passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(.5, .5, .5, 1.);

	id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

	id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor: passDescriptor];
	[commandEncoder setRenderPipelineState: pipeline];
	[commandEncoder setVertexBuffer: vertices offset: 0 atIndex: 0];
	[commandEncoder setVertexBuffer: colors offset: 0 atIndex: 1];
	[commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
	[commandEncoder endEncoding];
	[commandBuffer presentDrawable: drawable];
	[commandBuffer commit];*/

	timer_.Tick();
	scene_.Update(timer_.FrameMs(), timer_.TotalMs());
	id<CAMetalDrawable> drawable = [[metalView getMetalLayer] nextDrawable];
	id<MTLTexture> texture = drawable.texture;
	size_t commandBufferIndex = [renderer_ beginRender];
	size_t encoderIndex = [renderer_ startRenderPass: texture withCommandBuffer: commandBufferIndex];
	scene_.Render(encoderIndex);

	[renderer_ renderTo: drawable withCommandBuffer:commandBufferIndex];
}

- (void)setRepresentedObject:(id)representedObject {
	[super setRepresentedObject:representedObject];

	// Update the view, if already loaded.
}

- (void)mouseEntered:(NSEvent *)event {
	NSLog(@"mouse entered");
}

- (void)mouseExited:(NSEvent *)event {
	NSLog(@"mouse exited");
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
	NSLog(@"mouse dragged %f %f", event.locationInWindow.x, event.locationInWindow.y);

	if (event.modifierFlags & NSEventModifierFlagCommand)
		scene_.input.TranslateXZ(event.locationInWindow.x, -event.locationInWindow.y);
	else
		scene_.input.Rotate(event.locationInWindow.x, -event.locationInWindow.y);
}
- (void)rightMouseDragged:(NSEvent *)event {
	NSLog(@"mouse dragged %f %f", event.locationInWindow.x, event.locationInWindow.y);
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
