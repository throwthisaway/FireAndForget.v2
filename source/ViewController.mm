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
		scene_.input.Start(local_point.x, local_point.y);
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
		scene_.input.Start(local_point.x, local_point.y);
	}
}
- (void)mouseMoved:(NSEvent *)event {
	NSPoint pos = [event locationInWindow];
	NSPoint local_point = [metalView convertPoint:pos fromView:nil];
	local_point.y = metalView.bounds.size.height - local_point.y;
	if (DEBUGUI_PROCESSINPUT(ui::UpdateMousePos(local_point.x, local_point.y))) {
	}
}
- (void)mouseDragged:(NSEvent *)event {
	//NSLog(@"mouse dragged %f %f", event.locationInWindow.x, event.locationInWindow.y);
	NSPoint pos = [event locationInWindow];
	NSPoint local_point = [metalView convertPoint:pos fromView:nil];
	local_point.y = metalView.bounds.size.height - local_point.y;
	if (DEBUGUI_PROCESSINPUT(ui::UpdateMousePos(local_point.x, local_point.y))) {
		ui::UpdateKeyboardModifiers((event.modifierFlags & NSEventModifierFlagControl) == NSEventModifierFlagControl, (event.modifierFlags & NSEventModifierFlagOption) == NSEventModifierFlagOption, (event.modifierFlags & NSEventModifierFlagShift) == NSEventModifierFlagShift);
		ui::UpdateMouseButton(true, false, false);
		return;
	}
	if (event.modifierFlags & NSEventModifierFlagCommand)
		scene_.input.TranslateXZ(local_point.x, local_point.y);
	else
		scene_.input.Rotate(local_point.x, local_point.y);
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
	if (DEBUGUI_PROCESSINPUT(ui::UpdateMousePos(local_point.x, local_point.y))) {
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
namespace {
	constexpr int KeycodeMask(int X) { return (X | (1<<30)); }
	enum Scancodes {
		kScancodeUnknown =  0,

		kScancodeA =  4,
		kScancodeB =  5,
		kScancodeC =  6,
		kScancodeD =  7,
		kScancodeE =  8,
		kScancodeF =  9,
		kScancodeG =  10,
		kScancodeH =  11,
		kScancodeI =  12,
		kScancodeJ =  13,
		kScancodeK =  14,
		kScancodeL =  15,
		kScancodeM =  16,
		kScancodeN =  17,
		kScancodeO =  18,
		kScancodeP =  19,
		kScancodeQ =  20,
		kScancodeR =  21,
		kScancodeS =  22,
		kScancodeT =  23,
		kScancodeU =  24,
		kScancodeV =  25,
		kScancodeW =  26,
		kScancodeX =  27,
		kScancodeY =  28,
		kScancodeZ =  29,

		kScancode1 =  30,
		kScancode2 =  31,
		kScancode3 =  32,
		kScancode4 =  33,
		kScancode5 =  34,
		kScancode6 =  35,
		kScancode7 =  36,
		kScancode8 =  37,
		kScancode9 =  38,
		kScancode0 =  39,

		kScancodeReturn =  40,
		kScancodeEscape =  41,
		kScancodeBackspace =  42,
		kScancodeTab =  43,
		kScancodeSpace =  44,

		kScancodeMinus =  45,
		kScancodeEquals =  46,
		kScancodeLeftbracket =  47,
		kScancodeRightbracket =  48,
		kScancodeBackslash =  49,
		kScancodeNonushash =  50,
		kScancodeSemicolon =  51,
		kScancodeApostrophe =  52,
		kScancodeGrave =  53,
		kScancodeComma =  54,
		kScancodePeriod =  55,
		kScancodeSlash =  56,

		kScancodeCapslock =  57,

		kScancodeF1 =  58,
		kScancodeF2 =  59,
		kScancodeF3 =  60,
		kScancodeF4 =  61,
		kScancodeF5 =  62,
		kScancodeF6 =  63,
		kScancodeF7 =  64,
		kScancodeF8 =  65,
		kScancodeF9 =  66,
		kScancodeF10 =  67,
		kScancodeF11 =  68,
		kScancodeF12 =  69,

		kScancodePrintscreen =  70,
		kScancodeScrolllock =  71,
		kScancodePause =  72,
		kScancodeInsert =  73, /**< insert on PC, help on some Mac keyboards (but does send code 73, not 117) */
		kScancodeHome =  74,
		kScancodePageup =  75,
		kScancodeDelete =  76,
		kScancodeEnd =  77,
		kScancodePagedown =  78,
		kScancodeRight =  79,
		kScancodeLeft =  80,
		kScancodeDown =  81,
		kScancodeUp =  82,

		kScancodeNumlockclear =  83, /**< num lock on PC, clear on Mac keyboards */
		kScancodeKpDivide =  84,
		kScancodeKpMultiply =  85,
		kScancodeKpMinus =  86,
		kScancodeKpPlus =  87,
		kScancodeKpEnter =  88,
		kScancodeKp1 =  89,
		kScancodeKp2 =  90,
		kScancodeKp3 =  91,
		kScancodeKp4 =  92,
		kScancodeKp5 =  93,
		kScancodeKp6 =  94,
		kScancodeKp7 =  95,
		kScancodeKp8 =  96,
		kScancodeKp9 =  97,
		kScancodeKp0 =  98,
		kScancodeKpPeriod =  99,

		kScancodeNonusbackslash =  100,		/**< This is the additional key that ISO
											*   keyboards have over ANSI ones,
											*   located between left shift and Y.
											*   Produces GRAVE ACCENT and TILDE in a
											*   US or UK Mac layout, REVERSE SOLIDUS
											*   (backslash) and VERTICAL LINE in a
											*   US or UK Windows layout, and
											*   LESS-THAN SIGN and GREATER-THAN SIGN
											*   in a Swiss German, German, or French
											*   layout. */
		kScancodeApplication =  101, /**< windows contextual menu, compose */
		kScancodePower =  102,
		kScancodeKpEquals =  103,
		kScancodeF13 =  104,
		kScancodeF14 =  105,
		kScancodeF15 =  106,
		kScancodeF16 =  107,
		kScancodeF17 =  108,
		kScancodeF18 =  109,
		kScancodeF19 =  110,
		kScancodeF20 =  111,
		kScancodeF21 =  112,
		kScancodeF22 =  113,
		kScancodeF23 =  114,
		kScancodeF24 =  115,
		kScancodeExecute =  116,
		kScancodeHelp =  117,
		kScancodeMenu =  118,
		kScancodeSelect =  119,
		kScancodeStop =  120,
		kScancodeAgain =  121,   /**< redo */
		kScancodeUndo =  122,
		kScancodeCut =  123,
		kScancodeCopy =  124,
		kScancodePaste =  125,
		kScancodeFind =  126,
		kScancodeMute =  127,
		kScancodeVolumeup =  128,
		kScancodeVolumedown =  129,
		kScancodeKpComma =  133,
		kScancodeKpEqualsas400 =  134,

		kScancodeInternational1 =  135, /**< used on Asian keyboards */
		kScancodeInternational2 =  136,
		kScancodeInternational3 =  137, /**< Yen */
		kScancodeInternational4 =  138,
		kScancodeInternational5 =  139,
		kScancodeInternational6 =  140,
		kScancodeInternational7 =  141,
		kScancodeInternational8 =  142,
		kScancodeInternational9 =  143,
		kScancodeLang1 =  144, /**< Hangul/English toggle */
		kScancodeLang2 =  145, /**< Hanja conversion */
		kScancodeLang3 =  146, /**< Katakana */
		kScancodeLang4 =  147, /**< Hiragana */
		kScancodeLang5 =  148, /**< Zenkaku/Hankaku */
		kScancodeLang6 =  149, /**< reserved */
		kScancodeLang7 =  150, /**< reserved */
		kScancodeLang8 =  151, /**< reserved */
		kScancodeLang9 =  152, /**< reserved */

		kScancodeAlterase =  153, /**< Erase-Eaze */
		kScancodeSysreq =  154,
		kScancodeCancel =  155,
		kScancodeClear =  156,
		kScancodePrior =  157,
		kScancodeReturn2 =  158,
		kScancodeSeparator =  159,
		kScancodeOut =  160,
		kScancodeOper =  161,
		kScancodeClearagain =  162,
		kScancodeCrsel =  163,
		kScancodeExsel =  164,

		kScancodeKp00 =  176,
		kScancodeKp000 =  177,
		kScancodeThousandsseparator =  178,
		kScancodeDecimalseparator =  179,
		kScancodeCurrencyunit =  180,
		kScancodeCurrencysubunit =  181,
		kScancodeKpLeftparen =  182,
		kScancodeKpRightparen =  183,
		kScancodeKpLeftbrace =  184,
		kScancodeKpRightbrace =  185,
		kScancodeKpTab =  186,
		kScancodeKpBackspace =  187,
		kScancodeKpA =  188,
		kScancodeKpB =  189,
		kScancodeKpC =  190,
		kScancodeKpD =  191,
		kScancodeKpE =  192,
		kScancodeKpF =  193,
		kScancodeKpXor =  194,
		kScancodeKpPower =  195,
		kScancodeKpPercent =  196,
		kScancodeKpLess =  197,
		kScancodeKpGreater =  198,
		kScancodeKpAmpersand =  199,
		kScancodeKpDblampersand =  200,
		kScancodeKpVerticalbar =  201,
		kScancodeKpDblverticalbar =  202,
		kScancodeKpColon =  203,
		kScancodeKpHash =  204,
		kScancodeKpSpace =  205,
		kScancodeKpAt =  206,
		kScancodeKpExclam =  207,
		kScancodeKpMemstore =  208,
		kScancodeKpMemrecall =  209,
		kScancodeKpMemclear =  210,
		kScancodeKpMemadd =  211,
		kScancodeKpMemsubtract =  212,
		kScancodeKpMemmultiply =  213,
		kScancodeKpMemdivide =  214,
		kScancodeKpPlusminus =  215,
		kScancodeKpClear =  216,
		kScancodeKpClearentry =  217,
		kScancodeKpBinary =  218,
		kScancodeKpOctal =  219,
		kScancodeKpDecimal =  220,
		kScancodeKpHexadecimal =  221,

		kScancodeLCtrl =  224,
		kScancodeLShift =  225,
		kScancodeLAlt =  226, /**< alt, option */
		kScancodeLOSKey =  227, /**< windows, command (apple), meta */
		kScancodeRCtrl =  228,
		kScancodeRShift =  229,
		kScancodeRAlt =  230, /**< alt gr, option */
		kScancodeROSKey =  231, /**< windows, command (apple), meta */

		kScancodeMode =  257,

		kScancodeAudioNext =  258,
		kScancodeAudioPrev =  259,
		kScancodeAudioStop =  260,
		kScancodeAudioPlay =  261,
		kScancodeAudioMute =  262,
		kScancodeMediaselect =  263,
		kScancodeWWW =  264,
		kScancodeMail =  265,
		kScancodeCalculator =  266,
		kScancodeComputer =  267,
		kScancodeAcSearch =  268,
		kScancodeAcHome =  269,
		kScancodeAcBack =  270,
		kScancodeAcForward =  271,
		kScancodeAcStop =  272,
		kScancodeAcRefresh =  273,
		kScancodeAcBookmarks =  274,


		kScancodeBrightnessDown =  275,
		kScancodeBrightnessUp =  276,
		kScancodeDisplaySwitch =  277,
		kScancodeKbDillumToggle =  278,
		kScancodeKbDillumDown =  279,
		kScancodeKbDillumUp =  280,
		kScancodeEject =  281,
		kScancodeSleep =  282,

		kScancodeApp1 =  283,
		kScancodeApp2 =  284,

		kNumScancodes = 512		/**< not a key, just marks the number of scancodes
									 for array bounds */
	};

	enum Keycodes {
		kKeycodeUnknown =  0,

		kKeycodeReturn =  '\r',
		kKeycodeEscape =  '\033',
		kKeycodeBackspace =  '\b',
		kKeycodeTab =  '\t',
		kKeycodeSpace =  ' ',
		kKeycodeExclaim =  '!',
		kKeycodeQuotedbl =  '"',
		kKeycodeHash =  '#',
		kKeycodePercent =  '%',
		kKeycodeDollar =  '$',
		kKeycodeAmpersand =  '&',
		kKeycodeQuote =  '\'',
		kKeycodeLeftparen =  '(',
		kKeycodeRightparen =  ')',
		kKeycodeAsterisk =  '*',
		kKeycodePlus =  '+',
		kKeycodeComma =  ',',
		kKeycodeMinus =  '-',
		kKeycodePeriod =  '.',
		kKeycodeSlash =  '/',
		kKeycode0 =  '0',
		kKeycode1 =  '1',
		kKeycode2 =  '2',
		kKeycode3 =  '3',
		kKeycode4 =  '4',
		kKeycode5 =  '5',
		kKeycode6 =  '6',
		kKeycode7 =  '7',
		kKeycode8 =  '8',
		kKeycode9 =  '9',
		kKeycodeColon =  ':',
		kKeycodeSemicolon =  ';',
		kKeycodeLess =  '<',
		kKeycodeEquals = '=',
		kKeycodeGreater =  '>',
		kKeycodeQuestion =  '?',
		kKeycodeAt =  '@',

		kKeycodeLeftbracket =  '[',
		kKeycodeBackslash =  '\\',
		kKeycodeRightbracket =  ']',
		kKeycodeCaret =  '^',
		kKeycodeUnderscore =  '_',
		kKeycodeBackquote =  '`',
		kKeycodeA =  'a',
		kKeycodeB =  'b',
		kKeycodeC =  'c',
		kKeycodeD =  'd',
		kKeycodeE =  'e',
		kKeycodeF =  'f',
		kKeycodeG =  'g',
		kKeycodeH =  'h',
		kKeycodeI =  'i',
		kKeycodeJ =  'j',
		kKeycodeK =  'k',
		kKeycodeL =  'l',
		kKeycodeM =  'm',
		kKeycodeN =  'n',
		kKeycodeO =  'o',
		kKeycodeP =  'p',
		kKeycodeQ =  'q',
		kKeycodeR =  'r',
		kKeycodeS =  's',
		kKeycodeT =  't',
		kKeycodeU =  'u',
		kKeycodeV =  'v',
		kKeycodeW =  'w',
		kKeycodeX =  'x',
		kKeycodeY =  'y',
		kKeycodeZ =  'z',

		kKeycodeCapslock = KeycodeMask(kScancodeCapslock),

		kKeycodeF1 = KeycodeMask(kScancodeF1),
		kKeycodeF2 = KeycodeMask(kScancodeF2),
		kKeycodeF3 = KeycodeMask(kScancodeF3),
		kKeycodeF4 = KeycodeMask(kScancodeF4),
		kKeycodeF5 = KeycodeMask(kScancodeF5),
		kKeycodeF6 = KeycodeMask(kScancodeF6),
		kKeycodeF7 = KeycodeMask(kScancodeF7),
		kKeycodeF8 = KeycodeMask(kScancodeF8),
		kKeycodeF9 = KeycodeMask(kScancodeF9),
		kKeycodeF10 = KeycodeMask(kScancodeF10),
		kKeycodeF11 = KeycodeMask(kScancodeF11),
		kKeycodeF12 = KeycodeMask(kScancodeF12),

		kKeycodePrintscreen = KeycodeMask(kScancodePrintscreen),
		kKeycodeScrolllock = KeycodeMask(kScancodeScrolllock),
		kKeycodePause = KeycodeMask(kScancodePause),
		kKeycodeInsert = KeycodeMask(kScancodeInsert),
		kKeycodeHome = KeycodeMask(kScancodeHome),
		kKeycodePageup = KeycodeMask(kScancodePageup),
		kKeycodeDelete =  '\177',
		kKeycodeEnd = KeycodeMask(kScancodeEnd),
		kKeycodePagedown = KeycodeMask(kScancodePagedown),
		kKeycodeRight = KeycodeMask(kScancodeRight),
		kKeycodeLeft = KeycodeMask(kScancodeLeft),
		kKeycodeDown = KeycodeMask(kScancodeDown),
		kKeycodeUp = KeycodeMask(kScancodeUp),

		kKeycodeNumlockclear = KeycodeMask(kScancodeNumlockclear),
		kKeycodeKpDivide = KeycodeMask(kScancodeKpDivide),
		kKeycodeKpMultiply = KeycodeMask(kScancodeKpMultiply),
		kKeycodeKpMinus = KeycodeMask(kScancodeKpMinus),
		kKeycodeKpPlus = KeycodeMask(kScancodeKpPlus),
		kKeycodeKpEnter = KeycodeMask(kScancodeKpEnter),
		kKeycodeKp1 = KeycodeMask(kScancodeKp1),
		kKeycodeKp2 = KeycodeMask(kScancodeKp2),
		kKeycodeKp3 = KeycodeMask(kScancodeKp3),
		kKeycodeKp4 = KeycodeMask(kScancodeKp4),
		kKeycodeKp5 = KeycodeMask(kScancodeKp5),
		kKeycodeKp6 = KeycodeMask(kScancodeKp6),
		kKeycodeKp7 = KeycodeMask(kScancodeKp7),
		kKeycodeKp8 = KeycodeMask(kScancodeKp8),
		kKeycodeKp9 = KeycodeMask(kScancodeKp9),
		kKeycodeKp0 = KeycodeMask(kScancodeKp0),
		kKeycodeKpPeriod = KeycodeMask(kScancodeKpPeriod),

		kKeycodeApplication = KeycodeMask(kScancodeApplication),
		kKeycodePower = KeycodeMask(kScancodePower),
		kKeycodeKpEquals = KeycodeMask(kScancodeKpEquals),
		kKeycodeF13 = KeycodeMask(kScancodeF13),
		kKeycodeF14 = KeycodeMask(kScancodeF14),
		kKeycodeF15 = KeycodeMask(kScancodeF15),
		kKeycodeF16 = KeycodeMask(kScancodeF16),
		kKeycodeF17 = KeycodeMask(kScancodeF17),
		kKeycodeF18 = KeycodeMask(kScancodeF18),
		kKeycodeF19 = KeycodeMask(kScancodeF19),
		kKeycodeF20 = KeycodeMask(kScancodeF20),
		kKeycodeF21 = KeycodeMask(kScancodeF21),
		kKeycodeF22 = KeycodeMask(kScancodeF22),
		kKeycodeF23 = KeycodeMask(kScancodeF23),
		kKeycodeF24 = KeycodeMask(kScancodeF24),
		kKeycodeExecute = KeycodeMask(kScancodeExecute),
		kKeycodeHelp = KeycodeMask(kScancodeHelp),
		kKeycodeMenu = KeycodeMask(kScancodeMenu),
		kKeycodeSelect = KeycodeMask(kScancodeSelect),
		kKeycodeStop = KeycodeMask(kScancodeStop),
		kKeycodeAgain = KeycodeMask(kScancodeAgain),
		kKeycodeUndo = KeycodeMask(kScancodeUndo),
		kKeycodeCut = KeycodeMask(kScancodeCut),
		kKeycodeCopy = KeycodeMask(kScancodeCopy),
		kKeycodePaste = KeycodeMask(kScancodePaste),
		kKeycodeFind = KeycodeMask(kScancodeFind),
		kKeycodeMute = KeycodeMask(kScancodeMute),
		kKeycodeVolumeup = KeycodeMask(kScancodeVolumeup),
		kKeycodeVolumedown = KeycodeMask(kScancodeVolumedown),
		kKeycodeKpComma = KeycodeMask(kScancodeKpComma),
		kKeycodeKpEqualsas400 = KeycodeMask(kScancodeKpEqualsas400),

		kKeycodeAlterase = KeycodeMask(kScancodeAlterase),
		kKeycodeSysreq = KeycodeMask(kScancodeSysreq),
		kKeycodeCancel = KeycodeMask(kScancodeCancel),
		kKeycodeClear = KeycodeMask(kScancodeClear),
		kKeycodePrior = KeycodeMask(kScancodePrior),
		kKeycodeReturn2 = KeycodeMask(kScancodeReturn2),
		kKeycodeSeparator = KeycodeMask(kScancodeSeparator),
		kKeycodeOut = KeycodeMask(kScancodeOut),
		kKeycodeOper = KeycodeMask(kScancodeOper),
		kKeycodeClearagain = KeycodeMask(kScancodeClearagain),
		kKeycodeCrsel = KeycodeMask(kScancodeCrsel),
		kKeycodeExsel = KeycodeMask(kScancodeExsel),

		kKeycodeKp00 = KeycodeMask(kScancodeKp00),
		kKeycodeKp000 = KeycodeMask(kScancodeKp000),
		kKeycodeThousandsseparator = KeycodeMask(kScancodeThousandsseparator),
		kKeycodeDecimalseparator = KeycodeMask(kScancodeDecimalseparator),
		kKeycodeCurrencyunit = KeycodeMask(kScancodeCurrencyunit),
		kKeycodeCurrencysubunit = KeycodeMask(kScancodeCurrencysubunit),
		kKeycodeKpLeftparen = KeycodeMask(kScancodeKpLeftparen),
		kKeycodeKpRightparen = KeycodeMask(kScancodeKpRightparen),
		kKeycodeKpLeftbrace = KeycodeMask(kScancodeKpLeftbrace),
		kKeycodeKpRightbrace = KeycodeMask(kScancodeKpRightbrace),
		kKeycodeKpTab = KeycodeMask(kScancodeKpTab),
		kKeycodeKpBackspace = KeycodeMask(kScancodeKpBackspace),
		kKeycodeKpA = KeycodeMask(kScancodeKpA),
		kKeycodeKpB = KeycodeMask(kScancodeKpB),
		kKeycodeKpC = KeycodeMask(kScancodeKpC),
		kKeycodeKpD = KeycodeMask(kScancodeKpD),
		kKeycodeKpE = KeycodeMask(kScancodeKpE),
		kKeycodeKpF = KeycodeMask(kScancodeKpF),
		kKeycodeKpXor = KeycodeMask(kScancodeKpXor),
		kKeycodeKpPower = KeycodeMask(kScancodeKpPower),
		kKeycodeKpPercent = KeycodeMask(kScancodeKpPercent),
		kKeycodeKpLess = KeycodeMask(kScancodeKpLess),
		kKeycodeKpGreater = KeycodeMask(kScancodeKpGreater),
		kKeycodeKpAmpersand = KeycodeMask(kScancodeKpAmpersand),
		kKeycodeKpDblampersand = KeycodeMask(kScancodeKpDblampersand),
		kKeycodeKpVerticalbar = KeycodeMask(kScancodeKpVerticalbar),
		kKeycodeKpDblverticalbar = KeycodeMask(kScancodeKpDblverticalbar),
		kKeycodeKpColon = KeycodeMask(kScancodeKpColon),
		kKeycodeKpHash = KeycodeMask(kScancodeKpHash),
		kKeycodeKpSpace = KeycodeMask(kScancodeKpSpace),
		kKeycodeKpAt = KeycodeMask(kScancodeKpAt),
		kKeycodeKpExclam = KeycodeMask(kScancodeKpExclam),
		kKeycodeKpMemstore = KeycodeMask(kScancodeKpMemstore),
		kKeycodeKpMemrecall = KeycodeMask(kScancodeKpMemrecall),
		kKeycodeKpMemclear = KeycodeMask(kScancodeKpMemclear),
		kKeycodeKpMemadd = KeycodeMask(kScancodeKpMemadd),
		kKeycodeKpMemsubtract = KeycodeMask(kScancodeKpMemsubtract),
		kKeycodeKpMemmultiply = KeycodeMask(kScancodeKpMemmultiply),
		kKeycodeKpMemdivide = KeycodeMask(kScancodeKpMemdivide),
		kKeycodeKpPlusminus = KeycodeMask(kScancodeKpPlusminus),
		kKeycodeKpClear = KeycodeMask(kScancodeKpClear),
		kKeycodeKpClearentry = KeycodeMask(kScancodeKpClearentry),
		kKeycodeKpBinary = KeycodeMask(kScancodeKpBinary),
		kKeycodeKpOctal = KeycodeMask(kScancodeKpOctal),
		kKeycodeKpDecimal = KeycodeMask(kScancodeKpDecimal),
		kKeycodeKpHexadecimal = KeycodeMask(kScancodeKpHexadecimal),

		kKeycodeLCtrl = KeycodeMask(kScancodeLCtrl),
		kKeycodeLShift = KeycodeMask(kScancodeLShift),
		kKeycodeLAlt = KeycodeMask(kScancodeLAlt),
		kKeycodeLOSKey = KeycodeMask(kScancodeLOSKey),
		kKeycodeRCtrl = KeycodeMask(kScancodeRCtrl),
		kKeycodeRShift = KeycodeMask(kScancodeRShift),
		kKeycodeRAlt = KeycodeMask(kScancodeRAlt),
		kKeycodeROSKey = KeycodeMask(kScancodeROSKey),

		kKeycodeMode = KeycodeMask(kScancodeMode),

		kKeycodeAudioNext = KeycodeMask(kScancodeAudioNext),
		kKeycodeAudioPrev = KeycodeMask(kScancodeAudioPrev),
		kKeycodeAudioStop = KeycodeMask(kScancodeAudioStop),
		kKeycodeAudioPlay = KeycodeMask(kScancodeAudioPlay),
		kKeycodeAudioMute = KeycodeMask(kScancodeAudioMute),
		kKeycodeMediaselect = KeycodeMask(kScancodeMediaselect),
		kKeycodeWWW = KeycodeMask(kScancodeWWW),
		kKeycodeMail = KeycodeMask(kScancodeMail),
		kKeycodeCalculator = KeycodeMask(kScancodeCalculator),
		kKeycodeComputer = KeycodeMask(kScancodeComputer),
		kKeycodeAcSearch = KeycodeMask(kScancodeAcSearch),
		kKeycodeAcHome = KeycodeMask(kScancodeAcHome),
		kKeycodeAcBack = KeycodeMask(kScancodeAcBack),
		kKeycodeAcForward = KeycodeMask(kScancodeAcForward),
		kKeycodeAcStop = KeycodeMask(kScancodeAcStop),
		kKeycodeAcRefresh = KeycodeMask(kScancodeAcRefresh),
		kKeycodeAcBookmarks = KeycodeMask(kScancodeAcBookmarks),

		kKeycodeBrightnessDown = KeycodeMask(kScancodeBrightnessDown),
		kKeycodeBrightnessUp = KeycodeMask(kScancodeBrightnessUp),
		kKeycodeDisplaySwitch = KeycodeMask(kScancodeDisplaySwitch),
		kKeycodeKbDillumToggle = KeycodeMask(kScancodeKbDillumToggle),
		kKeycodeKbDillumDown = KeycodeMask(kScancodeKbDillumDown),
		kKeycodeKbDillumUp = KeycodeMask(kScancodeKbDillumUp),
		kKeycodeEject = KeycodeMask(kScancodeEject),
		kKeycodeSleep = KeycodeMask(kScancodeSleep)
	};
	int64_t ConvertKeyCode(unsigned short keyCode) {
		switch (keyCode) {
			case kVK_LeftArrow: return Keycodes::kKeycodeLeft;
			case kVK_RightArrow: return Keycodes::kKeycodeRight;
			case kVK_UpArrow: return Keycodes::kKeycodeUp;
			case kVK_DownArrow: return Keycodes::kKeycodeDown;
			case kVK_PageUp: return Keycodes::kKeycodePageup;
			case kVK_PageDown: return Keycodes::kKeycodePagedown;
			case kVK_Delete: return Keycodes::kKeycodeBackspace;
			case kVK_Home: return Keycodes::kKeycodeHome;
			case kVK_End: return Keycodes::kKeycodeEnd;
			case kVK_F1: return Keycodes::kKeycodeF1;
			case kVK_F2: return Keycodes::kKeycodeF2;
			case kVK_F3: return Keycodes::kKeycodeF3;
			case kVK_F4: return Keycodes::kKeycodeF4;
			case kVK_F5: return Keycodes::kKeycodeF5;
			case kVK_F6: return Keycodes::kKeycodeF6;
			case kVK_F7: return Keycodes::kKeycodeF7;
			case kVK_F8: return Keycodes::kKeycodeF8;
			case kVK_F9: return Keycodes::kKeycodeF9;
			case kVK_F10: return Keycodes::kKeycodeF10;
			case kVK_F11: return Keycodes::kKeycodeF11;
			case kVK_F12: return Keycodes::kKeycodeF12;
			case kVK_ANSI_0: return Keycodes::kKeycode0;
			default: return Keycodes::kKeycodeUnknown; // TODO: handle every key
		}
	}
}
- (void)keyDown:(NSEvent *)event {
	if (event.keyCode == kVK_Escape) {
		[[[self view] window] close];
		return;
	}
	DEBUGUI(ui::UpdateKeyboardModifiers((event.modifierFlags & NSEventModifierFlagControl) == NSEventModifierFlagControl, (event.modifierFlags & NSEventModifierFlagOption) == NSEventModifierFlagOption, (event.modifierFlags & NSEventModifierFlagShift) == NSEventModifierFlagShift));
	NSString* chars = [event characters];
	auto ch = [chars length] == 0 ? ConvertKeyCode(event.keyCode) : [chars characterAtIndex:0];
	DEBUGUI_PROCESSINPUT(ui::UpdateKeyboardInput(ch));
	if (!DEBUGUI_PROCESSINPUT(ui::UpdateKeyboard(ch, true))) {
		assert(ch < sizeof(keys) / sizeof(keys[0]));
		keys[ch] = true;
	}
}
- (void)keyUp:(NSEvent *) event {
	DEBUGUI(ui::UpdateKeyboardModifiers((event.modifierFlags & NSEventModifierFlagControl) == NSEventModifierFlagControl, (event.modifierFlags & NSEventModifierFlagOption) == NSEventModifierFlagOption, (event.modifierFlags & NSEventModifierFlagShift) == NSEventModifierFlagShift));
	NSString* chars = [event characters];
	auto ch = [chars length] == 0 ? ConvertKeyCode(event.keyCode) : [chars characterAtIndex:0];
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
