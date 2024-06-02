#include "Platform.h"

#if PLATFORM_MAC

#include "Memory/Memory.h"
#include "String/StringUtils.h"
#include "Uuid.h"
#include "Platform/Filesystem.h"
#include "Math/Math.h"
#include "Log.h"

#undef internal
#undef global

#include <Foundation/Foundation.h>
#include <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>
#include <mach/mach_time.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <termios.h>
#include <semaphore.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

@class ApplicationDelegate;
@class WindowDelegate;
@class ContentView;

STRUCT(MacPlatformState)
{
#if !HEADLESS
    ApplicationDelegate* AppDelegate;
    WindowDelegate* WindowDelegate;
    NSWindow* Window;
    ContentView* View;
    CAMetalLayer* MetalLayer;
    u8 ModifierKeyStates;
#endif

    bool bInitialized;
    bool bQuit;

    u64 ThreadID;
};

ENUM(MacModifierKey)
{
    MacModifierKey_LeftShift    = 0x01,
    MacModifierKey_RightShift   = 0x02,
    MacModifierKey_LeftControl  = 0x04,
    MacModifierKey_RightControl = 0x08,
    MacModifierKey_LeftOption   = 0x10,
    MacModifierKey_RightOption  = 0x20,
    MacModifierKey_LeftCommand  = 0x40,
    MacModifierKey_RightCommand = 0x80
};

#ifndef HEADLESS
static LinearAllocator GPlatformStateMemoryAllocator = {0};
static MacPlatformState* GMacPlatformState = NULL;

#define MACOS_LSHIFT_MASK    (1 << 1)
#define MACOS_RSHIFT_MASK    (1 << 2)
#define MACOS_LCTRL_MASK     (1 << 0)
#define MACOS_RCTRL_MASK     (1 << 13)
#define MACOS_LCOMMAND_MASK  (1 << 3)
#define MACOS_RCOMMAND_MASK  (1 << 4)
#define MACOS_LALT_MASK      (1 << 5)
#define MACOS_RALT_MASK      (1 << 6)

static void HandleModifierKey(u32 KeyCode, u32 KeyMask,
                              u32 NS_Left_KeyCode, u32 NS_Right_KeyCode,
                              u32 LeftKey, u32 RightKey,
                              u32 ModifierFlags,
                              u32 LeftMod, u32 RightMod,
                              u32 LeftMask, u32 RightMask)
{
    if (ModifierFlags & KeyMask)
    {
        // Check left variant
        if (ModifierFlags & LeftMask)
        {
            if (!(GMacPlatformState->ModifierKeyStates & LeftMod))
            {
                GMacPlatformState->ModifierKeyStates |= LeftMod;
            
                Input_ProcessKey(LeftKey, true);
            }
        }

        // Check right variant
        if (ModifierFlags & RightMask)
        {
            if (!(GMacPlatformState->ModifierKeyStates & RightMod))
            {
                GMacPlatformState->ModifierKeyStates |= RightMod;
            
                Input_ProcessKey(RightKey, true);
            }
        }
    }
    else
    {
        // Check left variant
        if (KeyCode == NS_Left_KeyCode)
        {
            if (GMacPlatformState->ModifierKeyStates & LeftMod)
            {
                GMacPlatformState->ModifierKeyStates &= ~LeftMod;
            
                Input_ProcessKey(LeftKey, false);
            }
        }

        // Check right variant
        if (KeyCode == NS_Right_KeyCode)
        {
            if (GMacPlatformState->ModifierKeyStates & RightMod)
            {
                GMacPlatformState->ModifierKeyStates &= ~RightMod;
            
                Input_ProcessKey(RightKey, false);
            }
        }
    }
}

static void Internal_HandleModifierKeys(u32 KeyCode, u32 ModifierFlags)
{
    // Shift
    HandleModifierKey(KeyCode, NSEventModifierFlagShift,
                      0x38, 0x3C,
                      Key_LeftShift, Key_RightShift,
                      ModifierFlags,
                      MacModifierKey_LeftShift, MacModifierKey_RightShift,
                      MACOS_LSHIFT_MASK, MACOS_RSHIFT_MASK);

    // Control
    HandleModifierKey(KeyCode, NSEventModifierFlagControl,
                      0x3B, 0x3E,
                      Key_LeftControl, Key_RightControl,
                      ModifierFlags,
                      MacModifierKey_LeftControl, MacModifierKey_RightControl,
                      MACOS_LCTRL_MASK, MACOS_RCTRL_MASK);

    // Option
    HandleModifierKey(KeyCode, NSEventModifierFlagOption,
                      0x3A, 0x3D,
                      Key_LeftAlt, Key_RightAlt,
                      ModifierFlags,
                      MacModifierKey_LeftOption, MacModifierKey_RightOption,
                      MACOS_LALT_MASK, MACOS_RALT_MASK);

    // Command
    HandleModifierKey(KeyCode, NSEventModifierFlagCommand,
                      0x37, 0x36,
                      Key_LeftCommand, Key_RightCommand,
                      ModifierFlags,
                      MacModifierKey_LeftCommand, MacModifierKey_RightCommand,
                      MACOS_LCOMMAND_MASK, MACOS_RCOMMAND_MASK);

    // Caps lock - handled differently
    if (KeyCode == 0x39)
    {
        if (ModifierFlags & NSEventModifierFlagCapsLock)
        {
            Input_ProcessKey(KeyCode, true);
        }
        else
        {
            Input_ProcessKey(KeyCode, false);
        }
    }
}

static EKey Internal_TranslateKeyCode(u32 KeyCode)
{
    switch (KeyCode)
    {
        case 0x00: return Key_A;
        case 0x01: return Key_S;
        case 0x02: return Key_D;
        case 0x03: return Key_F;
        case 0x04: return Key_H;
        case 0x05: return Key_G;
        case 0x06: return Key_Z;
        case 0x07: return Key_X;
        case 0x08: return Key_C;
        case 0x09: return Key_V;
        case 0x0B: return Key_B;
        case 0x0C: return Key_Q;
        case 0x0D: return Key_W;
        case 0x0E: return Key_E;
        case 0x0F: return Key_R;
        case 0x10: return Key_Y;
        case 0x11: return Key_T;
        case 0x12: return Key_1;
        case 0x13: return Key_2;
        case 0x14: return Key_3;
        case 0x15: return Key_4;
        case 0x16: return Key_6;
        case 0x17: return Key_5;
        case 0x18: return Key_Plus; // plus/equal
        case 0x19: return Key_9;
        case 0x1A: return Key_7;
        case 0x1B: return Key_Minus;
        case 0x1C: return Key_8;
        case 0x1D: return Key_0;
        case 0x1E: return Key_RightBracket;
        case 0x1F: return Key_O;
        case 0x20: return Key_U;
        case 0x21: return Key_LeftBracket;
        case 0x22: return Key_I;
        case 0x23: return Key_P;
        case 0x25: return Key_L;
        case 0x26: return Key_J;
        case 0x27: return Key_Quote;
        case 0x28: return Key_K;
        case 0x29: return Key_Semicolon;
        case 0x2A: return Key_Backslash;
        case 0x2B: return Key_Comma;
        case 0x2C: return Key_Slash;
        case 0x2D: return Key_N;
        case 0x2E: return Key_M;
        case 0x2F: return Key_Period;
        case 0x32: return Key_Grave;
        case 0x41: return Key_Decimal;
        case 0x43: return Key_Multiply;
        case 0x45: return Key_Add;
        case 0x47: return Key_Numlock;
        case 0x4B: return Key_Divide;
        case 0x4C: return Key_Enter;
        case 0x4E: return Key_Subtract;
        case 0x51: return Key_Equal;
        case 0x52: return Key_Numpad0;
        case 0x53: return Key_Numpad1;
        case 0x54: return Key_Numpad2;
        case 0x55: return Key_Numpad3;
        case 0x56: return Key_Numpad4;
        case 0x57: return Key_Numpad5;
        case 0x58: return Key_Numpad6;
        case 0x59: return Key_Numpad7;
        case 0x5B: return Key_Numpad8;
        case 0x5C: return Key_Numpad9;
        case 0x24: return Key_Enter;
        case 0x30: return Key_Tab;
        case 0x31: return Key_Space;
        case 0x33: return Key_Delete;
        case 0x35: return Key_Escape;
        case 0x36: return Key_RightCommand;
        case 0x37: return Key_LeftCommand;
        case 0x38: return Key_LeftShift;
        case 0x39: return Key_Capital;
        case 0x3A: return Key_LeftAlt;
        case 0x3B: return Key_LeftControl;
        case 0x3C: return Key_RightShift;
        case 0x3D: return Key_RightAlt;
        case 0x3E: return Key_RightControl;
        case 0x3F: return Key_Function;
        case 0x7A: return Key_F1;
        case 0x78: return Key_F2;
        case 0x63: return Key_F3;
        case 0x76: return Key_F4;
        case 0x60: return Key_F5;
        case 0x61: return Key_F6;
        case 0x62: return Key_F7;
        case 0x64: return Key_F8;
        case 0x65: return Key_F9;
        case 0x6D: return Key_F10;
        case 0x67: return Key_F11;
        case 0x6F: return Key_F12;
        case 0x69: return Key_Print;
        case 0x6B: return Key_F14;
        case 0x71: return Key_F15;
        case 0x6A: return Key_F16;
        case 0x40: return Key_F17;
        case 0x4F: return Key_F18;
        case 0x50: return Key_F19;
        case 0x5A: return Key_F20;
        case 0x73: return Key_Home;
        case 0x77: return Key_End;
        case 0x72: return Key_Insert;
        case 0x74: return Key_PageUp;
        case 0x79: return Key_PageDown;
        case 0x7B: return Key_Left;
        case 0x7C: return Key_Right;
        case 0x7D: return Key_Down;
        case 0x7E: return Key_Up;
        default:   return Key_Null;
    }
}

@interface WindowDelegate : NSObject<NSWindowDelegate>
{
    MacPlatformState* PlatformState;
}

- (instancetype)initWithState:(MacPlatformState*)InitState;

@end // WindowDelegate

@interface ContentView : NSView<NSTextInputClient>
{
    NSWindow* Window;
    NSTrackingArea* TrackingArea;
    NSMutableAttributedString* MarkedText;
}

- (instancetype)initWithWindow:(NSWindow*)InitWindow;

@end // ContentView

@implementation ContentView

- (instancetype)initWithWindow:(NSWindow*)InitWindow
{
    self = [super init];
    if (self)
    {
        Window = InitWindow;
        //TrackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds] options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow) owner:self userInfo:nil];
        //[self addTrackingArea:TrackingArea];
        //MarkedText = [[NSMutableAttributedString alloc] init];
    }

    return self;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)Event
{
    return YES;
}

- (void)mouseDown:(NSEvent*)Event
{
    Input_ProcessMouseButton(MouseButton_Left, true);
}

- (void)mouseUp:(NSEvent*)Event
{
    Input_ProcessMouseButton(MouseButton_Left, false);
}

- (void)mouseDragged:(NSEvent*)Event
{
    [self mouseMoved:Event];
}

- (void)mouseMoved:(NSEvent*)Event
{
    NSPoint Location = [Event locationInWindow];
    Input_ProcessMouseMove((i32)Location.x, (i32)Location.y);
}

- (void)rightMouseDown:(NSEvent*)Event
{
    Input_ProcessMouseButton(MouseButton_Right, true);
}

- (void)rightMouseUp:(NSEvent*)Event
{
    Input_ProcessMouseButton(MouseButton_Right, false);
}

- (void)rightMouseDragged:(NSEvent*)Event
{
    [self mouseMoved:Event];
}

- (void)otherMouseDown:(NSEvent*)Event
{
    Input_ProcessMouseButton(MouseButton_Middle, true);
}

- (void)otherMouseUp:(NSEvent*)Event
{
    Input_ProcessMouseButton(MouseButton_Middle, false);
}

- (void)otherMouseDragged:(NSEvent*)Event
{
    [self mouseMoved:Event];
}

- (void)flagsChanged:(NSEvent*)Event
{
    u32 KeyCode = [Event keyCode];
    u64 ModifierFlags = [Event modifierFlags];
    Internal_HandleModifierKeys(KeyCode, (u32)ModifierFlags);
}

- (void)keyDown:(NSEvent*)Event
{
    u32 KeyCode = [Event keyCode];
    
    EKey Key = Internal_TranslateKeyCode(KeyCode);
    Input_ProcessKey(Key, true);
}

- (void)keyUp:(NSEvent*)Event
{
    u32 KeyCode = [Event keyCode];
    
    EKey Key = Internal_TranslateKeyCode(KeyCode);
    Input_ProcessKey(Key, false);
}

- (void)scrollWheel:(NSEvent*)Event
{
    f64 Delta = [Event deltaY];
    Input_ProcessMouseWheelMove((i16)Delta);
}

- (void)insertText:(id)Text replacementRange:(NSRange)ReplacementRange
{
}

- (void)setMarkedText:(id)Text selectedRange:(NSRange)SelectedRange replacementRange:(NSRange)ReplacementRange
{
}

- (void)unmarkText
{
}

static const NSRange EmptyRange = {NSNotFound, 0};

- (NSRange)selectedRange
{
    return EmptyRange;
}

- (NSRange)markedRange
{
    return EmptyRange;
}

- (BOOL)hasMarkedText
{
    return NO;
}

- (nullable NSAttributedString*)attributedSubstringForProposedRange:(NSRange)Range actualRange:(NSRangePointer)ActualRange
{
    return nil;
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText
{
    return [NSArray array];
}

- (NSRect)firstRectForCharacterRange:(NSRange)Range actualRange:(nullable NSRangePointer)ActualRange
{
    return NSZeroRect;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)Point
{
    return 0;
}

@end // ContentView

@interface ApplicationDelegate : NSObject<NSApplicationDelegate>
{
}
@end // ApplicationDelegate

@implementation ApplicationDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)Notification
{
    //NSLog(@"Application did finish launching");
    @autoreleasepool
    {
        NSEvent* Event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined 
                                  location:NSZeroPoint
                                  modifierFlags:0
                                  timestamp:0
                                  windowNumber:0
                                  context:nil
                                  subtype:0
                                  data1:0
                                  data2:0];

        [NSApp postEvent:Event atStart:YES];
    }

    [NSApp stop:nil];
}
@end // ApplicationDelegate

@implementation WindowDelegate
- (instancetype)initWithState:(MacPlatformState*)InitState
{
    self = [super init];
    if (self)
    {
        PlatformState = InitState;
        GMacPlatformState->bQuit = false;
    }

    return self;
}

- (BOOL)windowShouldClose:(id)Sender
{
    GMacPlatformState->bQuit = true;

    EventContext Event = EventContext_Null();
    Event_Fire(EventCode_ApplicationQuit, NULL, Event);

    return YES;
}

- (void)windowDidResize:(NSNotification*)Notification
{
    CGSize ViewSize = GMacPlatformState->View.bounds.size;
    NSSize NewDrawableSize = [GMacPlatformState->View convertSizeToBacking:ViewSize];

    GMacPlatformState->MetalLayer.drawableSize = NewDrawableSize;
    GMacPlatformState->MetalLayer.contentsScale = GMacPlatformState->View.window.backingScaleFactor;

    EventContext Event = EventContext_Null();
    Event.Data.u16[0] = (u16)NewDrawableSize.width;
    Event.Data.u16[1] = (u16)NewDrawableSize.height;

    Event_Fire(EventCode_WindowResize, NULL, Event);
}

- (void)windowDidMiniaturize:(NSNotification*)Notification
{
    EventContext Event = EventContext_Null();
    Event.Data.u16[0] = 0;
    Event.Data.u16[1] = 0;

    Event_Fire(EventCode_WindowResize, NULL, Event);

    [GMacPlatformState->Window miniaturize:nil];
}

- (void)windowDidDeminiaturize:(NSNotification*)Notification
{
    CGSize ViewSize = GMacPlatformState->View.bounds.size;
    NSSize NewDrawableSize = [GMacPlatformState->View convertSizeToBacking:ViewSize];

    GMacPlatformState->MetalLayer.drawableSize = NewDrawableSize;
    GMacPlatformState->MetalLayer.contentsScale = GMacPlatformState->View.window.backingScaleFactor;

    EventContext Event = EventContext_Null();
    Event.Data.u16[0] = (u16)NewDrawableSize.width;
    Event.Data.u16[1] = (u16)NewDrawableSize.height;

    Event_Fire(EventCode_WindowResize, NULL, Event);

    [GMacPlatformState->Window deminiaturize:nil];
}

@end // WindowDelegate

#endif

static void LogLastError(const String Prefix)
{
    StringLocal(Message, 4096);
    String_Copy(&Message, CStr(strerror(errno)));

    LOG_ERROR("%S\n        errno %i\n        Reason: %S\n", Prefix, errno, Message);
}

bool Platform_Startup(void* State, const String ApplicationName, i32 X, i32 Y, u32 Width, u32 Height)
{
    #ifndef HEADLESS
    LinearAllocator_Create(Platform_GetMemoryRequirement(), State, &GPlatformStateMemoryAllocator);

    GMacPlatformState = LinearAllocator_Allocate(&GPlatformStateMemoryAllocator, sizeof(MacPlatformState));
    GMacPlatformState->bInitialized = true;
    
    @autoreleasepool
    {
        [NSApplication sharedApplication];

        ApplicationDelegate* AppDelegate = [[ApplicationDelegate alloc] init];
        GMacPlatformState->AppDelegate = AppDelegate;

        if (!GMacPlatformState->AppDelegate)
        {
            LOG_ERROR("Failed to create application delegate");
            return false;
        }

        [NSApp setDelegate:GMacPlatformState->AppDelegate];

        // Window delegate creation
        GMacPlatformState->WindowDelegate = [[WindowDelegate alloc] initWithState:GMacPlatformState];
        if (!GMacPlatformState->WindowDelegate)
        {
            LOG_ERROR("Failed to create window delegate");
            return false;
        }

        // window creation
        GMacPlatformState->Window = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(X, Y, Width, Height)
            styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskUnifiedTitleAndToolbar)
            backing:NSBackingStoreBuffered
            defer:NO];

        if (!GMacPlatformState->Window)
        {
            LOG_ERROR("Failed to create window");
            return false;
        }

        // View creation
        GMacPlatformState->View = [[ContentView alloc] initWithWindow:GMacPlatformState->Window];
        [GMacPlatformState->View setWantsLayer:YES];

        // Layer creation
        GMacPlatformState->MetalLayer = [CAMetalLayer layer];
        if (!GMacPlatformState->MetalLayer)
        {
            LOG_ERROR("Failed to create Metal layer");
            return false;
        }

        // setting window properties
        [GMacPlatformState->Window setTitle:@(ApplicationName.Data)];
        [GMacPlatformState->Window setLevel:NSNormalWindowLevel];
        [GMacPlatformState->Window setContentView:GMacPlatformState->View];
        [GMacPlatformState->Window makeFirstResponder:GMacPlatformState->View];
        [GMacPlatformState->Window setDelegate:GMacPlatformState->WindowDelegate];
        [GMacPlatformState->Window setAcceptsMouseMovedEvents:YES];
        [GMacPlatformState->Window setRestorable:NO];

        if (![[NSRunningApplication currentApplication] isFinishedLaunching])
        {
            [NSApp run];
        }

        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        [NSApp activateIgnoringOtherApps:YES];
        [GMacPlatformState->Window makeKeyAndOrderFront:nil];

        // Handle content scaling
        GMacPlatformState->MetalLayer.bounds = GMacPlatformState->View.bounds;
        GMacPlatformState->MetalLayer.contentsScale = GMacPlatformState->View.window.backingScaleFactor;
        
        GMacPlatformState->MetalLayer.opaque = YES;
        GMacPlatformState->MetalLayer.presentsWithTransaction = NO;
        GMacPlatformState->MetalLayer.displaySyncEnabled = NO;

        // It is important to set the drawable size to the actual backing pixels
        // When rendering full-screen, we can skip the macOS compositor if the size matches the screen
        GMacPlatformState->MetalLayer.drawableSize = [GMacPlatformState->View convertSizeToBacking:GMacPlatformState->View.bounds.size];

        GMacPlatformState->MetalLayer.contentsScale = GMacPlatformState->View.window.backingScaleFactor;
        LOG("Content Scale: %f", GMacPlatformState->MetalLayer.contentsScale);

        [GMacPlatformState->View setLayer:GMacPlatformState->MetalLayer];

        EventContext Event = EventContext_Null();
        Event.Data.u16[0] = (u16)GMacPlatformState->MetalLayer.drawableSize.width;
        Event.Data.u16[1] = (u16)GMacPlatformState->MetalLayer.drawableSize.height;
        Event_Fire(EventCode_WindowResize, NULL, Event);
    }
    #endif

    return true;
}

void Platform_Shutdown(void)
{
    #ifndef HEADLESS
    GMacPlatformState->bInitialized = false;
    GMacPlatformState->bQuit = true;

    @autoreleasepool
    {
        [GMacPlatformState->Window orderOut:nil];
        [GMacPlatformState->Window setDelegate:nil];
        [GMacPlatformState->WindowDelegate release];

        [GMacPlatformState->View release];
        GMacPlatformState->View = nil;

        [GMacPlatformState->Window close];
        GMacPlatformState->Window = nil;

        [NSApp setDelegate:nil];
        [GMacPlatformState->AppDelegate release];
        GMacPlatformState->AppDelegate = nil;
    }
    #endif
}

u64 Platform_GetMemoryRequirement(void)
{
    return sizeof(MacPlatformState);
}

bool Platform_PushMessages(void)
{
    #ifndef HEADLESS
    @autoreleasepool
    {
        NSEvent* Event;

        while (1)
        {
            Event = [NSApp 
                    nextEventMatchingMask:NSEventMaskAny
                    untilDate:[NSDate distantPast]
                    inMode:NSDefaultRunLoopMode
                    dequeue:YES];

            if (!Event)
                break;

            [NSApp sendEvent:Event];
        }

        return !GMacPlatformState->bQuit;
    }
    #endif

    return false;
}

void Platform_ShowWindow(void)
{
    /*
    @autoreleasepool
    {
        [GMacPlatformState->Window makeKeyAndOrderFront:nil];
    }
    */
}

void Platform_HideWindow(void)
{
    /*
    @autoreleasepool
    {
        [GMacPlatformState->Window orderOut:nil];
    }
    */
}

static void Internal_SignalHandler(int signal)
{
    Platform_Shutdown();

    exit(1);
}

void Platform_PreInitialize(void)
{
    Platform_GetClockFrequency();

    struct sigaction act = {0};
    act.sa_handler = &Internal_SignalHandler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGKILL, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
}

f64 Platform_GetClockFrequency(void)
{
    return 0;
}

void* Platform_GetWindowHandle(void)
{
    #ifndef HEADLESS
    return GMacPlatformState->WindowDelegate;
    #endif

    return nullptr; // good idea?
}

NO_RETURN void Platform_Abort(u32 ExitCode)
{
    exit((i32)ExitCode);
}

// TODO: BUILD_LIB define
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-prototypes"
static String GArgV[128] = {0};
static i32 GArgC = 0;
static char** GEnv = NULL;

static String GProgramName = { 0 };
static char GEmptyBuffer[16] = {0};

void pre_main(int argc, char* argv[], char* env[])
{
    GArgC = argc;
    GEnv  = env;

    for (u16 i = 0; i < 128; i++)
    {
        GArgV[i].Data = GEmptyBuffer;
        GArgV[i].Length = 0;
        GArgV[i].Capacity = 15;
    }

    for (int i = 1; i < argc; ++i)
    {
        GArgV[i-1].Data = argv[i];
        GArgV[i-1].Length = String_GetLength_Ex(argv[i], UINT16_MAX);
        GArgV[i-1].Capacity = GArgV[i-1].Length;
    }

    GProgramName.Data = argv[0];
    GProgramName.Length = String_GetLength_Ex(argv[0], UINT16_MAX);
    GProgramName.Capacity = GProgramName.Length;
}
#pragma clang diagnostic pop

StringArray Platform_GetCommandLineArgs(void)
{
    StringArray Args = {0};
    Args.Num = (u32)(GArgC-1 <= 0 ? 0 : (GArgC-1 < 128 ? GArgC-1 : 128));
    Args.List = GArgV;
    return Args;
}

void* Platform_MemAlloc(u64 Size)
{
    return malloc(Size);
}

void* Platform_MemAllocZero(u64 Size)
{
    void* Memory = malloc(Size);
    memset(Memory, 0, Size);
    return Memory;
}

void* Platform_MemReAlloc(const void* Block, u64 Size)
{
    return realloc((void*)Block, Size);
}

void  Platform_MemFree(const void* Block)
{
    free((void*)Block);
}

void* Platform_MemZero(void* Block, u64 Size)
{
    return memset(Block, 0, Size);
}

void* Platform_MemCopy(void* restrict Dest, const void* restrict Source, u64 Size)
{
    return memcpy(Dest, Source, Size);
}

void* Platform_MemMove(void* restrict Dest, const void* restrict Source, u64 Size)
{
    return memmove(Dest, Source, Size);
}

void* Platform_MemSet(void* Dest, i32 Value, u64 Size)
{
    return memset(Dest, Value, Size);
}

bool Platform_MemEqual(const void* Block1, const void* Block2, u64 Size)
{
    return memcmp(Block1, Block2, Size) == 0;
}

void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError)
{
    Platform_ConsoleWrite_CustomLength(Message, String_GetLength(Message), 0, false);
}

void Platform_ConsoleWrite_CustomLength(const char* Message, u64 Length, u8 Color, bool bIsError)
{
    static const char* colors[] = {"0;37", "0;32", "1;33", "1;31", "0;41", "0;37"};

    bool bIgnoreNewLine = Color == 4 && Message[Length-1] == '\n';
    if (UNLIKELY(bIgnoreNewLine))
        Length--;

    fprintf(UNLIKELY(bIsError) ? stderr : stdout, "\033[%.*sm%.*s\033[0m", 4, LIKELY(Color < 6) ? colors[Color] : "0:37", (i32)Length, Message);
    //printf("\033[%.*sm%.*s\033[0m", 4, LIKELY(Color < 6) ? colors[Color] : "0:37", (i32)Length, Message);

    if (UNLIKELY(bIgnoreNewLine))
        printf("\n");

    fflush(stdout);
}

PlatformHandle Platform_CreateThread(const String Name, u32* OutThreadID, u32 (*ThreadEntryPoint)(void* ThreadParameter), void* UserData)
{
    return 0;
}

PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory)
{
    String Command;
    StringLocal(Copy, MAX_PATH_LENGTH*2);
    if (WorkingDirectory.Length > 0)
    {
        String_Append(&Copy, S("cd \""));
        String_Append(&Copy, WorkingDirectory);
        String_EatPathSeparatorsInlineFromEnd(&Copy);
        String_Append(&Copy, S("\"; "));
        String_Append(&Copy, CmdLine);

        Command = Copy;
    }
    else
    {
        Command = CmdLine;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        LogLastError(S("fork() failed"));
    }
    else if (pid > 0) // parent path
    {
        return pid;
    }
    else // child path
    {
        execvp("/bin/sh", (char*[]){"sh", "-c", Command.Data, NULL});
        exit(0);
    }

    return pid;
}

PlatformHandle Platform_RunCommand_Ex(const String CmdLine, const String WorkingDirectory, PlatformPipe* StdOutPipe)
{
    String Command;
    StringLocal(Copy, MAX_PATH_LENGTH*2);
    if (WorkingDirectory.Length > 0)
    {
        String_Append(&Copy, S("cd \""));
        String_Append(&Copy, WorkingDirectory);
        String_EatPathSeparatorsInlineFromEnd(&Copy);
        String_Append(&Copy, S("\"; "));
        String_Append(&Copy, CmdLine);

        Command = Copy;
    }
    else
    {
        Command = CmdLine;
    }

    i32 PipeData[2] = {0};
    if (pipe(PipeData) != 0)
    {
        LogLastError(S("pipe() failed"));
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        close(PipeData[0]);
        close(PipeData[1]);
        LogLastError(S("fork() failed"));
    }
    else if (pid > 0) // parent path
    {
        close(PipeData[0]);
        close(PipeData[1]);
        return pid;
    }
    else // child path
    {
        close(PipeData[0]);
        dup2(PipeData[1], STDOUT_FILENO);

        (*StdOutPipe)[0] = PipeData[0]; // read pipe
        (*StdOutPipe)[1] = PipeData[1]; // write pipe

        execvp("/bin/sh", (char*[]){"sh", "-c", Command.Data, NULL});
        exit(0);
    }

    return pid;
}

bool Platform_FindProgram(String ProgramName)
{
    return Platform_FindFile_Ex(ProgramName, S(""), NULL);
}

bool Platform_FindProgram_Ex(String FileName, String* OutFilePath)
{
    return Platform_FindFile_Ex(FileName, S(""), OutFilePath);
}

bool Platform_FindFile(String FileName, String ExtensionWithDot)
{
    return Platform_FindFile_Ex(FileName, ExtensionWithDot, NULL);
}

bool Platform_FindFile_Ex(String FileName, String ExtensionWithDot, String* OutFilePath)
{
    char* Path = getenv("PATH");
    const String PathStr = CStr(Path);

    bool bFound = false;

    StringLocal(P, MAX_PATH_LENGTH);
    u32 Offset = 0;
    u32 Len = 0;
    for (u32 i = 0; i < PathStr.Length; i++)
    {
        if (PathStr.Data[i] == ':' || i == PathStr.Length-1) // end of an entry, process it
        {
            String_Copy(&P, StrSlice(StrShiftF(PathStr, Offset).Data, Len));
            Offset = i+1;
            Len = 0;

            if (bFound)
                break;

            DIR* dir = opendir(P.Data);
            if (dir == NULL)
            {
                continue;
            }

            struct dirent* Entry = NULL;
            while ((Entry = readdir(dir)))
            {
                if (Entry->d_name[0] == '.' && 
                    (!Entry->d_name[1] || (Entry->d_name[1] == '.' && !Entry->d_name[2])))
                {
                    continue;
                }

                const String EntryName = CStr(Entry->d_name);

                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, P, EntryName);
                
                if (Entry->d_type != DT_DIR)
                {
                    u64 FileSize = 0;
                    struct stat filestat = {0};
                    stat(FullPath.Data, &filestat);
                    FileSize = (u64)filestat.st_size;

                    if (FileSize > 0)
                    {
                        // is this file an executeable?
                        if (((filestat.st_mode & S_IXUSR) || (filestat.st_mode & S_IXGRP) || (filestat.st_mode & S_IXOTH)))
                        {
                            //LOG("%S", EntryName);
                            if (ExtensionWithDot.Length > 0)
                            {
                                u32 LastDot = 0;
                                if (String_IndexOfLastChar(EntryName, '.', &LastDot))
                                {
                                    if (String_IsEqual(FileName, StrSlice(EntryName.Data, LastDot), false) &&
                                        String_EndsWith(FileName, ExtensionWithDot, true))
                                    {
                                        bFound = true;
                                    }
                                }
                            }
                            else
                            {
                                if (String_IsEqual(FileName, EntryName, false))
                                {
                                    bFound = true;
                                }
                            }

                            if (bFound)
                            {
                                if (OutFilePath)
                                    String_Copy(OutFilePath, FullPath);

                                break;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            Len++;
        }
    }

    //TEMP_SCRATCH(Find)
    {
        //StringArray Paths = String_ParseIntoArray_IntoExistingBuffer(&PathsBuffer, PathStr, ':', 0, 256);
        //StringArray Paths = String_ParseIntoArray(Scratch_Find.Allocator, PathStr, ':', 0, 256);
        /*
        for each_str (P, Paths)
        {
            if (bFound)
                break;

            DIR* dir = opendir(P->Data);
            if (dir == NULL)
            {
                continue;
            }

            struct dirent* Entry = NULL;
            while ((Entry = readdir(dir)))
            {
                if (Entry->d_name[0] == '.' && 
                    (!Entry->d_name[1] || (Entry->d_name[1] == '.' && !Entry->d_name[2])))
                {
                    continue;
                }

                const String EntryName = CStr(Entry->d_name);

                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, *P, EntryName);
                
                if (Entry->d_type != DT_DIR)
                {
                    u64 FileSize = 0;
                    struct stat filestat = {0};
                    stat(FullPath.Data, &filestat);
                    FileSize = (u64)filestat.st_size;

                    if (FileSize > 0)
                    {
                        // is this file an executeable?
                        if (((filestat.st_mode & S_IXUSR) || (filestat.st_mode & S_IXGRP) || (filestat.st_mode & S_IXOTH)))
                        {
                            //LOG("%S", EntryName);
                            if (ExtensionWithDot.Length > 0)
                            {
                                u32 LastDot = 0;
                                if (String_IndexOfLastChar(EntryName, '.', &LastDot))
                                {
                                    if (String_IsEqual(FileName, StrSlice(EntryName.Data, LastDot), false) &&
                                        String_EndsWith(FileName, ExtensionWithDot, true))
                                    {
                                        bFound = true;
                                    }
                                }
                            }
                            else
                            {
                                if (String_IsEqual(FileName, EntryName, false))
                                {
                                    bFound = true;
                                }
                            }

                            if (bFound)
                            {
                                if (OutFilePath)
                                    String_Copy(OutFilePath, FullPath);

                                break;
                            }
                        }
                    }
                }
            }
        }
        */
    }

    return bFound;
}

u32 Platform_GetExitCodeForProcess(PlatformHandle Handle)
{
    if (Handle == 0)
        return 0;

    i32 PidStatus;
    pid_t pid = waitpid(Handle, &PidStatus, 0); // if you call this twice on the same pid, linux wont return the same exit code like windows does... sadge :(
    if (pid == -1)
    {
        return 0;
    }

    return WEXITSTATUS(PidStatus);
}

u32 Platform_WaitForProcessAndGetExitCode(PlatformHandle Handle)
{
    if (Handle == 0)
        return 0;

    i32 PidStatus;
    pid_t pid = waitpid(Handle, &PidStatus, 0);
    if (pid == -1)
    {
        return 0;
    }

    return WEXITSTATUS(PidStatus);
}

void Platform_WaitForHandle(PlatformHandle Handle, i32 Milliseconds)
{
    waitpid(Handle, NULL, 0);
}

u32 Platform_WaitForMultipleHandles(PlatformHandle* Handles, u32 NumHandles, i32 Milliseconds, bool bWaitAll)
{
    if (bWaitAll)
    {
        for (u32 i = 0; i < NumHandles; i++)
        {
            waitpid(Handles[i], NULL, 0);
        }

        return 0;
    }

    for (u32 i = 0; i < NumHandles; i++)
    {
        Platform_WaitForHandle(Handles[i], Milliseconds);
        if (!bWaitAll)
        {
            return i;
        }
    }

    return 0;
}

void Platform_CloseHandle(PlatformHandle Handle)
{
    close(Handle);
}

bool Platform_IsValidHandle(const PlatformHandle Handle)
{
    return Handle >= 0;
}

u64 Platform_GetCriticalSectionMemoryRequirement(void)
{
    return 4;
}

void Platform_InitializeCriticalSection(PlatformCriticalSection OutCriticalSection)
{
}

void Platform_DeleteCriticalSection(PlatformCriticalSection CriticalSection)
{
}

void Platform_EnterCriticalSection(PlatformCriticalSection CriticalSection)
{
}

void Platform_ExitCriticalSection(PlatformCriticalSection CriticalSection)
{
}

bool Platform_CreateMutex(const String Name, PlatformMutex* OutMutex)
{
    if ((NEVER(Name.Length == 0)) || (NEVER(OutMutex == NULL)))
    {
        return false;
    }

    u32 Diff = Name.Length > 30 ? Name.Length - 30 : 0; // 31 is max name length for posix semaphores
    String ClampedName = StrShiftF(Name, Diff);

    sem_t* Semaphore = sem_open(ClampedName.Data, O_CREAT|O_EXCL, 0644, 1);
    if (Semaphore == SEM_FAILED)
    {
        if (errno == EACCES || errno == EEXIST)
        {
            return false;
        }

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to create mutex %S"), Prefix.Capacity, ClampedName);
        LogLastError(Prefix);
        return false;
    }

    OutMutex->Handle = Semaphore;
    OutMutex->Name = ClampedName;
    return true;
}

bool Platform_ReleaseMutex(PlatformMutex* Mutex)
{
    if ((NEVER(Mutex->Name.Length == 0)) || (NEVER(Mutex == NULL)))
    {
        return false;
    }

    if (sem_close(Mutex->Handle) == -1)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to release mutex %S"), Prefix.Capacity, Mutex->Name);
        LogLastError(Prefix);
        return false;
    }

    // for some fucking reason semaphores have kernel persistence, so we need to unlink them, otherwise the user will have to shutdown their machine
    // which is why again windows dominates the market, go look at the code in Platform_Windows.c!!!
    if (sem_unlink(Mutex->Name.Data) == -1)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to unlink mutex %S"), Prefix.Capacity, Mutex->Name);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

f64 Platform_GetAbsoluteTime(void)
{
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);

    u64 Time = mach_absolute_time();
    f64 Nano = ((f64)Time * (f64)info.numer) / (f64)info.denom;

    return Nano/1.0e9;
}

SystemTime Platform_GetSystemLocalTime(void)
{
    time_t mytime = time(0);
    ctime(&mytime);

    struct tm* lt = localtime(&mytime);

    SystemTime t;
    t.Year = (u16)lt->tm_year + 1900;
    t.Month = (u16)lt->tm_mon + 1;
    t.DayOfWeek = 0;
    t.Day = (u16)lt->tm_mday;
    t.Hour = (u16)lt->tm_hour;
    t.Minute = (u16)lt->tm_min;
    t.Second = (u16)lt->tm_sec;
    t.Millisecond = 0;

    return t;
}

void Platform_Sleep(f64 ms)
{
        if (ms > 0)
        {
            mach_timebase_info_data_t info;
            mach_timebase_info(&info);

            u64 Time = mach_absolute_time();
            f64 Start = (((f64)Time * (f64)info.numer) / (f64)info.denom) / 1.0e-9; // 1e-9

            f64 Target = ms/1000.0;

            while (1)
            {
                mach_timebase_info(&info);

                Time = mach_absolute_time();
                f64 Now = (((f64)Time * (f64)info.numer) / (f64)info.denom) / 1.0e-9; // 1e-9

                if ((Now-Start) >= Target)
                    break;
            }
        }
}

void Platform_ShowCursor(bool bShow)
{
    //UNIMPLEMENTED;
}

void Platform_GetMousePosition(f32* X, f32* Y)
{
    UNIMPLEMENTED;
}

u64 Platform_GetCurrentThreadID(void)
{
    return 0;
}

u64 Platform_GetMainThreadID(void)
{
    return (u64)getpid();
}

void Platform_GetWorkingDirectory(String* OutPath)
{
    getcwd(OutPath->Data, MAX_PATH_LENGTH);
    OutPath->Length = String_GetLength_Ex(OutPath->Data, MAX_PATH_LENGTH);
}

bool Platform_GetEnvironmentVariableValue(String Name, String* OutVariable)
{
    StringLocal(NameCopy, MAX_PATH_LENGTH);
    String_Copy(&NameCopy, Name);

    char* Value = getenv(NameCopy.Data);
    if (Value == NULL)
    {
        return false;
    }

    String_Copy(OutVariable, CStr(Value));
    return true;
}

bool Platform_DoesEnvironmentVariableExist(String Name)
{
    StringLocal(NameCopy, MAX_PATH_LENGTH);
    String_Copy(&NameCopy, Name);

    char* Value = getenv(NameCopy.Data);
    if (Value == NULL)
    {
        return false;
    }

    return true;
}

bool Platform_CaptureStackTrace(LinearAllocator* Arena, TArray(StackTraceData)* OutInfo)
{
    UNIMPLEMENTED;
    return false;
}

u32 Platform_GetNumLogicalProcessors(void)
{
    //return [[NSProcessInfo processInfo] processorCount];

    u32 NumProcessors = (u32)sysconf(_SC_NPROCESSORS_ONLN);
    return NumProcessors;
}

bool Platform_GetAccountName(String* OutName)
{
    return Platform_GetUserName(OutName);
}

bool Platform_GetUserName(String* OutName)
{
    struct passwd pwd = {0};
    struct passwd* result = NULL;
    char Buffer[4096] = {0};

    getpwuid_r(getuid(), &pwd, Buffer, 4096, &result);
    if (result == NULL)
    {
        StringLocal(Message, 512);
        String_Format(&Message, S("Failed to get user name"), Message.Capacity);
        LogLastError(Message);
        return false;
    }

    String_Copy(OutName, CStr(pwd.pw_name));
    return true;
}

bool Platform_GetUserDirectory(String* OutDirectory)
{
    struct passwd pwd = {0};
    struct passwd* result = NULL;
    char Buffer[4096] = {0};

    getpwuid_r(getuid(), &pwd, Buffer, 4096, &result);
    if (result == NULL)
    {
        return false;
    }

    String_Copy(OutDirectory, CStr(pwd.pw_dir));
    return true;
}

bool Platform_GetCurrentProcessName(String* OutName)
{
    // todo: from cmdline args
    return false;
}

u64 Platform_GetCurrentProcessID(void)
{
    return (u64)getpid();
}

u32 Platform_GetConsoleProcessCount(void)
{
    // TODO
    return 0;
}

bool Platform_GetThreadName(void* ThreadHandle, String* OutName)
{
    UNIMPLEMENTED;
    return false;
}

void* Platform_GetDeviceContext(void)
{
    UNIMPLEMENTED;
    return nullptr;
}

bool Platform_IsProgramRunning(const String ProgramName)
{
    LOG_WARNING("Platform_IsProgramRunning() is not implemented on this platform");
    /// TODO
    return false;
}


bool Filesystem_Open(const String FilePath, u32 Mode, FileHandle* OutHandle)
{
    String ModeStr = String_Null();

    if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) != 0)) // read and write
    {
        ModeStr = S("a+");
    }
    else if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) == 0)) // read only
    {
        ModeStr = S("r");
    }
    else if (((Mode & FileMode_Read) == 0) && ((Mode & FileMode_Write) != 0)) // write only
    {
        ModeStr = S("w");
    }
    else
    {
        LOG_WARNING("Invalid mode passed (%u) while trying to open file \"%S\"", Mode, FilePath);
        return false;
    }

    u32 LastSlash = 0;
    String_IndexOfLastPathSlash(FilePath, &LastSlash);

    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, StrSlice(FilePath.Data, LastSlash));

    Filesystem_OpenDirectory(Copy);

    FILE* File = fopen(FilePath.Data, ModeStr.Data); // refactor to just use open() instead of fopen()
    if (!File)
    {
        StringLocal(Message, MAX_PATH_LENGTH);
        String_Format(&Message, S("Failed to open file \"%S\""), MAX_PATH_LENGTH, FilePath);
        LogLastError(Message);
        return false;
    }

    ASSERT(OutHandle != NULL);

    OutHandle->Data = File;

    return true;
}

bool Filesystem_NewFile(const String FilePath)
{
    FileHandle f = {0};
    bool bSuccess = Filesystem_Open(FilePath, FileMode_Write, &f);
    Filesystem_Close(&f);

    return bSuccess;
}

bool Filesystem_DeleteFile(String FilePath)
{
    UNIMPLEMENTED;
    return false;
}

bool Filesystem_Open_MemoryMapped(const String FilePath, u32 Mode, FileHandle* OutHandle, u8** OutData, u64* OutSize)
{
    if (OutSize)
        *OutSize = 0;

    if (OutData)
        *OutData = NULL;

    if (!IsValidFileHandle(OutHandle))
    {
        if (!Filesystem_Open(FilePath, Mode, OutHandle))
            return false;
    }

    int ProtectFlags = 0;

    if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) != 0)) // read and write
    {
        ProtectFlags = PROT_READ | PROT_WRITE;
    }
    else if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) == 0)) // read only
    {
        ProtectFlags = PROT_READ;
    }
    else if (((Mode & FileMode_Read) == 0) && ((Mode & FileMode_Write) != 0)) // write only
    {
        ProtectFlags = PROT_WRITE;
    }

    u64 Size = 0;
    Filesystem_GetFileSize(OutHandle, &Size);

    void* Address = mmap(NULL, Size, ProtectFlags, MAP_SHARED, fileno(OutHandle->Data), 0);

    if (Address == MAP_FAILED)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to memory map file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return false;
    }

    OutHandle->Data2 = Address;

    if (OutData)
        *OutData = (u8*)Address;

    if (OutSize) 
        *OutSize = Size;

    return 0;
}

bool Filesystem_OpenDirectory(const String FilePath)
{
    if (Filesystem_DoesDirectoryExist(FilePath))
        return true;
    
    bool bAnySuccess = false;
    bool bFoundPathSeparator = false;
    u32 NextSlashIndex = 0;

    do
    {
        bFoundPathSeparator = false;

        for (u32 i = NextSlashIndex; i < FilePath.Length; i++)
        {
            if (FilePath.Data[i] == '/' || FilePath.Data[i] == '\\' || FilePath.Length-1 == i)
            {
                bFoundPathSeparator = true;

                StringLocal(BaseDirectory, MAX_PATH_LENGTH);
                if (FilePath.Length-1 == i)
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i+1));
                else
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i));

                NextSlashIndex = i+1;

                bool bDirectoryCreated = Filesystem_DoesDirectoryExist(BaseDirectory) || BaseDirectory.Length == 0;
                if (!bDirectoryCreated)
                {
                    i32 ErrorCode = mkdir(BaseDirectory.Data, 0700);
                    if (ErrorCode == -1)
                    {
                        StringLocal(Prefix, MAX_PATH_LENGTH);
                        String_Format(&Prefix, S("Failed to open directory \"%S\""), Prefix.Capacity, BaseDirectory);
                        LogLastError(Prefix);
                        return false;
                    }

                    bAnySuccess = true;
                }

                break;
            }
        }
    }
    while (bFoundPathSeparator);

    return bAnySuccess;
}

bool Filesystem_OpenDirectory_Ex(const String FilePath, FileHandle* OutHandle)
{
    return Filesystem_OpenDirectory(FilePath);
}

bool Filesystem_Close(FileHandle* Handle)
{
    bool bFailedUnmap = false;
    if (Handle->Data2)
    {
        Handle->Data2 = NULL;
        u64 Size = 0;
        Filesystem_GetFileSize(Handle, &Size);
        if (munmap(Handle->Data2, Size) == -1)
        {
            StringLocal(Path, MAX_PATH_LENGTH);
            Filesystem_GetFilePath(Handle, &Path);

            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to unmap memory for file \"%S\""), Prefix.Capacity, Path);
            LogLastError(Prefix);
            bFailedUnmap = true;
        }
    }

    if (IsValidFileHandle(Handle))
    {
        fclose(Handle->Data);
        *Handle = FileHandle_Null();
        return !bFailedUnmap;
    }
    
    return false;
}

bool Filesystem_Seek(const FileHandle* Handle, i64 Offset)
{
    i32 ErrorCode = fseek(Handle->Data, Offset, SEEK_CUR);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Seek failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekFromBeginning(const FileHandle* Handle, u64 Offset)
{
    i32 ErrorCode = fseek(Handle->Data, (i32)Offset, SEEK_SET);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekFromBeginning failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekFromEnd(const FileHandle* Handle, u64 Offset)
{
    i32 ErrorCode = fseek(Handle->Data, (i32)Offset, SEEK_END);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekFromEnd failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekToBeginning(const FileHandle* Handle)
{
    i32 ErrorCode = fseek(Handle->Data, 0, SEEK_SET);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekToBeginning failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekToEnd(const FileHandle* Handle)
{
    i32 ErrorCode = fseek(Handle->Data, 0, SEEK_END);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekToEnd failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

u64  Filesystem_GetCurrentFilePosition(const FileHandle* Handle)
{
    return (u64)ftell(Handle->Data);
}

u64  Filesystem_GetLastWriteTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat(FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve last write time for file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (u64)FileStat.st_mtime;
}

u64  Filesystem_GetLastAccessTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat(FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve last access time for file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (u64)FileStat.st_atime;
}

FileTimeData  Filesystem_GetFileTime(const String FilePath)
{
    FileTimeData a = {0};

    if (!Filesystem_DoesFileExist(FilePath))
        return a;

    a.CreationTime = Filesystem_GetCreationTime(FilePath);
    a.LastAccessTime = Filesystem_GetLastAccessTime(FilePath);
    a.LastWriteTime = Filesystem_GetLastWriteTime(FilePath);
    return a;
}

#ifdef HAVE_ST_BIRTHTIME
#define birthtime(x) x.st_birthtime
#else
#define birthtime(x) x.st_ctime
#endif

u64  Filesystem_GetCreationTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat(FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve creation time for file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (u64)birthtime(FileStat);
}

u64  Filesystem_GetLastWriteTimeH(const FileHandle* Handle)
{
    //fstat(fileno(Handle->Data), &FileStat);
    // TODO: something better
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);
    return Filesystem_GetLastWriteTime(Path);
}

u64  Filesystem_GetLastAccessTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);
    return Filesystem_GetLastAccessTime(Path);
}

u64  Filesystem_GetCreationTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);
    return Filesystem_GetCreationTime(Path);
}

FileTimeData  Filesystem_GetFileTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);

    FileTimeData a = {0};
    a.CreationTime = Filesystem_GetCreationTime(Path);
    a.LastAccessTime = Filesystem_GetLastAccessTime(Path);
    a.LastWriteTime = Filesystem_GetLastWriteTime(Path);
    return a;
}

bool Filesystem_ReadPipe(PlatformPipe Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(Handle[0] == -1)) return false;
    if (NEVER(Handle[1] == -1)) return false;

    i64 BytesRead = read(Handle[0], OutData, DataSize);
    if (BytesRead < 0)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read pipe from handle -> Read: %i | Write: %i"), Prefix.Capacity, Handle[0], Handle[1]);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = (u64)BytesRead;

    return true;
}

bool Filesystem_Read(const FileHandle* Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    u64 BytesRead = fread(OutData, 1, DataSize, Handle->Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return true;
}

bool Filesystem_ReadEntireFile(const FileHandle* Handle, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    Filesystem_SeekToBeginning(Handle);
    
    u64 Size = 0;
    Filesystem_GetFileSize(Handle, &Size);

    u64 BytesRead = fread(OutData, 1, Size, Handle->Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to entire read file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return true;
}

bool Filesystem_ReadLine(const FileHandle* Handle, String* LineBuffer)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (NEVER(LineBuffer == NULL)) return false;
    if (NEVER(LineBuffer->Data == NULL || LineBuffer->Data == String_Null().Data)) return false;

    u64 CurrentPosition = Filesystem_GetCurrentFilePosition(Handle);

    u64 FileSize = 0;
    Filesystem_GetFileSize(Handle, &FileSize);
    if (CurrentPosition >= FileSize)
    {
        Filesystem_SeekToBeginning(Handle);
        return false;
    }

    char TempBuffer[8192] = {0};
    u64 BytesRead = fread(TempBuffer, 1, 8191, Handle->Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read line for file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    // todo: move to core?
    /////////////////
    u32 Counter = 0;
    u32 FilePointerOffset = 0;
    for (u32 i = 0; i < BytesRead; i++)
    {
        if (TempBuffer[i] == '\0' || TempBuffer[i] == '\n' || TempBuffer[i] == '\r')
        {
            FilePointerOffset = Counter;

            if (TempBuffer[i] == '\r' && TempBuffer[i+1] == '\n') // todo: bounds check
                FilePointerOffset += 2;
            else
                FilePointerOffset++;

            TempBuffer[i] = 0;

            break;
        }

        Counter++;
    }

    if (FilePointerOffset == 0) // did not find a new line char, possibly at end of file
    {
        FilePointerOffset = Counter;
    }

    if (FilePointerOffset == 0)
    {
        return false;
    }

    u32 MaxLength = Min(LineBuffer->Capacity, 8192);
    u32 LineLength = Min(MaxLength-1, Counter);

    if (LineLength > 0)
    {
        String_Copy(LineBuffer, StrSlice(TempBuffer, LineLength));
    }
    else
    {
        String_Empty(LineBuffer);
    }

    Filesystem_SeekFromBeginning(Handle, CurrentPosition + FilePointerOffset);

    return true;
}

bool Filesystem_ReadLine_Backwards(const FileHandle* Handle, String* LineBuffer)
{
    UNIMPLEMENTED;
    return false;
}

bool Filesystem_Write(const FileHandle* Handle, u64 DataSize, const void* Data, u64* OutBytesWritten)
{
    ASSERT(IsValidFileHandle(Handle));

    if (DataSize == 0)
        return false;

    Filesystem_SeekToBeginning(Handle);

    u64 BytesWritten = fwrite(Data, 1, DataSize, Handle->Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write to file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_WriteLine(const FileHandle* Handle, const String Text, u64* OutBytesWritten)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    Filesystem_SeekToEnd(Handle);

    u64 BytesWritten = fwrite(Text.Data, 1, Text.Length, Handle->Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_WriteLineFormatted(const FileHandle* Handle, const String Text, u64* OutBytesWritten, ...)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    Filesystem_SeekToEnd(Handle);
    
    va_list Args;
    va_start(Args, OutBytesWritten);
    StringLocal(Buffer, 32768);
    String_FormatV(&Buffer, Text, 32768, Args);
    va_end(Args);

    u64 BytesWritten = fwrite(Buffer.Data, 1, Buffer.Length, Handle->Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_DoesFileExist(const String FilePath)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, FilePath);

    if (access(Copy.Data, F_OK) == 0)
    {
        return true;
    }
    
    return false;
}

bool Filesystem_DoesDirectoryExist(const String FilePath)
{
    DIR* Found = opendir(FilePath.Data);
    if (!Found)
    {
        return false;
    }

    return true;
}

bool Filesystem_GetFileSize(const FileHandle* File, u64* OutSize)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(File, &Path);

    struct stat filestat = {0};
    stat(Path.Data, &filestat);
    *OutSize = (u64)filestat.st_size;
    return true;
}

bool Filesystem_GetFilePath(const FileHandle* Handle, String* OutPath)
{
    if (!IsValidFileHandle(Handle))
        return false;

    char Path[PATH_MAX] = {0};
    if (fcntl(fileno(Handle->Data), F_GETPATH, Path) == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve file path for file handle"), Prefix.Capacity);
        LogLastError(Prefix);
        return false;
    }

    String_Copy(OutPath, CStr(Path));
    return true;
}

bool Filesystem_IsPathRelative(const String Path)
{
    return Path.Data[0] != '/';
}

bool Filesystem_IsFile(const String Path)
{
    struct stat filestat = {0};
    stat(Path.Data, &filestat);
    return S_ISREG(filestat.st_mode);
}

bool Filesystem_IsDirectory(const String Path)
{
    struct stat filestat = {0};
    stat(Path.Data, &filestat);
    return S_ISDIR(filestat.st_mode);
}

// todo: move into platform core?
bool Filesystem_IsNewer(const String PathA, const String PathB)
{
    u64 a = Filesystem_GetLastWriteTime(PathA);
    u64 b = Filesystem_GetLastWriteTime(PathB);
    return a > b;
}

bool Filesystem_IsOlder(const String PathA, const String PathB)
{
    u64 a = Filesystem_GetLastWriteTime(PathA);
    u64 b = Filesystem_GetLastWriteTime(PathB);
    return a < b;
}

bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, *OutFullPath);

    char* Result = realpath(Copy.Data, OutFullPath->Data);
    if (Result == NULL)
    {
        StringLocal(Format, MAX_PATH_LENGTH);
        String_Format(&Format, S("Failed to convert \"%S\" to an absolute path"), MAX_PATH_LENGTH, Copy);
        LogLastError(Format);
        return false;
    }

    OutFullPath->Length = String_GetLength(Result);

    return true;
}

static void Internal_IterateDirectory(const String BasePath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    struct dirent* entry = NULL;
    DIR* dp = opendir(BasePath.Data);
    if (!dp)
    {
        StringLocal(Message, MAX_PATH_LENGTH);
        String_Format(&Message, S("Failed to iterate directory for path \"%S\""), MAX_PATH_LENGTH, BasePath);
        LogLastError(Message);
        return;
    }
    
    while ((entry = readdir(dp)))
    {
        if (entry->d_type != DT_REG && entry->d_type != DT_DIR)
        {
            continue;
        }

        if (entry->d_name[0] == '.' && 
            (!entry->d_name[1] || (entry->d_name[1] == '.' && !entry->d_name[2])))
        {
            continue;
        }

        //LOG("%S", CStr(entry->d_name));

        const String EntryName = CStr(entry->d_name);

        StringLocal(FullPath, MAX_PATH_LENGTH);
        String_BuildPath(&FullPath, BasePath, EntryName);
        
        StringLocal(RelativePath, MAX_PATH_LENGTH);

        if (entry->d_type == DT_DIR)
        {
            String_BuildPath(&RelativePath, DirectoryPath, EntryName);

            bool bResult = Callback(FullPath, RelativePath, EntryName, 0, true, UserData);
            if (!bResult) break;

            if (bRecursive)
            {
                Internal_IterateDirectory(FullPath, RelativePath, Callback, true, UserData);
            }
        }
        else
        {
            u64 FileSize = 0;
            struct stat filestat = {0};
            stat(FullPath.Data, &filestat);
            FileSize = (u64)filestat.st_size;

            if (DirectoryPath.Length > 0)
            {
                String_BuildPath(&RelativePath, DirectoryPath, EntryName);
            }
            else
            {
                String_Copy(&RelativePath, EntryName);
            }

            bool bResult = Callback(FullPath, RelativePath, EntryName, FileSize, false, UserData);
            if (!bResult) break;
        }
    }
    
    closedir(dp);
}

void Filesystem_IterateDirectory(const String BasePath, DirectoryIterator Callback, bool bRecursive)
{
    Internal_IterateDirectory(BasePath, S(""), Callback, bRecursive, NULL);
}

void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    Internal_IterateDirectory(BasePath, S(""), Callback, bRecursive, UserData);
}

bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive)
{
    // TODO: do not use the shell for this
    if (FilePath.Length == 0 || (FilePath.Length == 1 && FilePath.Data[0] == '/' ))
        return false;

    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("rm -f "));
    if (bRecursive)
        String_Append(&Cmd, S("-r \""));
    String_Append(&Cmd, FilePath);
    String_AppendPathSeparator_Checked(&Cmd);
    String_AppendChar(&Cmd, '"');
    String_Append(&Cmd, Wildcard);
    String_Append(&Cmd, S(" 2> /dev/null"));
    i32 Result = system(Cmd.Data);
    return Result == 0;
}

bool Filesystem_DeleteDirectory(const String DirectoryPath)
{
    UNIMPLEMENTED;
    return false;
}

bool Filesystem_Copy(const String Source, const String Destination)
{
    UNIMPLEMENTED;
    return false;
}

bool Filesystem_ArePathsCommon(String PathA, String PathB)
{
    bool bPrefixMatch = String_StartsWith(PathB, PathA, true);
    
    return bPrefixMatch;
}

bool Filesystem_SanitizeQuotes(String* Dest, const String Path)
{
    bool bHasQuote = false;
    for (u32 i = 0; i < Path.Length; i++)
    {
        char c = Path.Data[i];
        if (c == '"' && bHasQuote)
        {
            // ignore all subsequent quotes
            continue;
        }

        String_AppendChar(Dest, c);

        if (c == '"')
        {
            bHasQuote = true;
        }
    }

    if (bHasQuote)
    {
        String_AppendChar(Dest, '"');
    }

    return Dest->Length > 0;
}

Uuid UUID_Generate(void)
{
    uuid_t id;
    uuid_generate(id);

    return *(Uuid*)id;
}

bool UUID_IsEqual(Uuid First, Uuid Second)
{
    //return Platform_MemEqual(&First, &Second, sizeof(Uuid));

    unsigned char* a = (unsigned char*)&First;
    unsigned char* b = (unsigned char*)&Second;

    const bool bSame = uuid_compare(a, b) == 0;
    return bSame;
}

void UUID_ToString(Uuid ID, String* OutString)
{
    StringLocal(Temp, GUID_LENGTH);

    unsigned char* a = (unsigned char*)&ID;
    uuid_unparse(a, Temp.Data);

    String_Copy(OutString, Temp);
}

Uuid UUID_FromString(const String IDString)
{
    uuid_t id;
    uuid_parse(IDString.Data, id);

    return *(Uuid*)id;
}

bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        LogLastError(S("Failed to get terminal dimensions"));
        return false;
    }

    *OutRows = w.ws_row;
    *OutColumns = w.ws_col;

    return true;
}

#endif
