#import <Cocoa/Cocoa.h>

#import "DialView.h"

#include "Bundle.h"
#include "Dial.h"

#include <stdlib.h>

/* A window with an appetite: the theme, the numerals on the face and the
   hourly motto under it all come out of Contents/Resources, and the program
   finds them from its own executable path (see Bundle.c). Delete one of
   those files and the app says so and quits - a bundle missing its resources
   is a broken program, not a smaller one. */

static NSArray* LinesFromText(const char* Text, int MaxLines)
{
    NSMutableArray* Lines = [NSMutableArray array];

    for (int i = 0; i < MaxLines; i++)
    {
        int Length = 0;
        const char* Line = Dial_LineAt(Text, i, &Length);

        if (Line == NULL)
        {
            break;
        }

        NSString* Text8 = [[NSString alloc] initWithBytes:Line length:(NSUInteger)Length encoding:NSUTF8StringEncoding];
        [Lines addObject:Text8 != nil ? Text8 : @""];
        [Text8 release];
    }

    return Lines;
}

static void ShowAlert(NSString* Message, NSString* Detail)
{
    NSAlert* Alert = [[NSAlert alloc] init];
    [Alert setAlertStyle:NSAlertStyleCritical];
    [Alert setMessageText:Message];
    [Alert setInformativeText:Detail];
    [Alert runModal];
    [Alert release];
}

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate
{
    NSWindow* Window;
    DialView* View;
}

- (void)dealloc
{
    [Window release];
    [View release];

    [super dealloc];
}

/* Reads the three resource files and hands them to the view. Returns NO after
   telling the user what is missing. */
- (BOOL)loadResources
{
    BOOL bSuccess = NO;

    char Directory[BUNDLE_PATH_MAX];
    if (!Bundle_FindResourceDirectory(Directory, sizeof(Directory)))
    {
        ShowAlert(@"Sundial cannot find itself", @"The OS would not say where this executable is.");
    }
    else
    {
        char* ThemeText   = Bundle_ReadTextFile(Directory, "theme.conf");
        char* NumeralText = Bundle_ReadTextFile(Directory, "numerals.txt");
        char* MottoText   = Bundle_ReadTextFile(Directory, "mottos.txt");

        if (ThemeText == NULL || NumeralText == NULL || MottoText == NULL)
        {
            ShowAlert(@"Sundial is missing its resources",
                      [NSString stringWithFormat:@"theme.conf, numerals.txt and mottos.txt should be in\n%s\n\n"
                                                  "The bundle is incomplete - build again to stage them.", Directory]);
        }
        else
        {
            DialTheme Theme;
            Dial_DefaultTheme(&Theme);
            Dial_ApplyTheme(ThemeText, &Theme);

            [View applyTheme:&Theme
                    numerals:LinesFromText(NumeralText, 12)
                      mottos:LinesFromText(MottoText, 24)];

            [Window setTitle:[View dialTitle]];

            bSuccess = YES;
        }

        free(ThemeText);
        free(NumeralText);
        free(MottoText);
    }

    return bSuccess;
}

/* Cmd-R. The files are read at run time, so editing theme.conf inside the
   .app and hitting this is enough - no rebuild, no relaunch. */
- (void)reloadResources:(id)Sender
{
    (void)Sender;

    if (![self loadResources])
    {
        [NSApp terminate:nil];
    }
}

/* A menu bar, built in code - the alternative is a nib, and a nib is a
   resource this example would then have to explain. */
- (void)buildMenuBar
{
    NSMenu* MenuBar = [[NSMenu alloc] init];
    NSMenuItem* AppMenuItem = [[NSMenuItem alloc] init];
    [MenuBar addItem:AppMenuItem];
    [NSApp setMainMenu:MenuBar];

    NSMenu* AppMenu = [[NSMenu alloc] init];
    [AppMenu addItemWithTitle:@"About Sundial" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [AppMenu addItem:[NSMenuItem separatorItem]];
    [AppMenu addItemWithTitle:@"Reload Resources" action:@selector(reloadResources:) keyEquivalent:@"r"];
    [AppMenu addItem:[NSMenuItem separatorItem]];
    [AppMenu addItemWithTitle:@"Quit Sundial" action:@selector(terminate:) keyEquivalent:@"q"];
    [AppMenuItem setSubmenu:AppMenu];

    [AppMenu release];
    [AppMenuItem release];
    [MenuBar release];
}

- (void)applicationDidFinishLaunching:(NSNotification*)Notification
{
    (void)Notification;

    [self buildMenuBar];

    NSRect Frame = NSMakeRect(0.0, 0.0, 460.0, 540.0);
    Window = [[NSWindow alloc] initWithContentRect:Frame
                                         styleMask:NSWindowStyleMaskTitled |
                                                   NSWindowStyleMaskClosable |
                                                   NSWindowStyleMaskMiniaturizable |
                                                   NSWindowStyleMaskResizable
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    [Window setMinSize:NSMakeSize(360.0, 420.0)];
    [Window center];

    View = [[DialView alloc] initWithFrame:Frame];
    [Window setContentView:View];

    if (![self loadResources])
    {
        [NSApp terminate:nil];
    }

    [Window makeKeyAndOrderFront:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)Application
{
    (void)Application;

    return YES;
}

@end

int main(void)
{
    @autoreleasepool
    {
        NSApplication* Application = [NSApplication sharedApplication];

        /* Bundled, macOS reads this from Info.plist; run bare out of Build/
           there is no plist to read, and this is what still gets the app a
           Dock icon, a menu bar and a window it can focus. */
        [Application setActivationPolicy:NSApplicationActivationPolicyRegular];

        AppDelegate* Delegate = [[AppDelegate alloc] init];
        [Application setDelegate:Delegate];
        [Application run];
        [Delegate release];
    }

    return 0;
}
