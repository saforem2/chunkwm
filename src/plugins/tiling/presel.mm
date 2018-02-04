#import <Cocoa/Cocoa.h>
#include "presel.h"

#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
#define NSWindowStyleMaskBorderless NSBorderlessWindowMask
#endif

inline void
DrawLine(int W, int X1, int Y1, int X2, int Y2)
{
    NSBezierPath *Line = [NSBezierPath bezierPath];
    [Line setLineWidth:W];
    [Line moveToPoint:NSMakePoint(X1, Y1)];
    [Line lineToPoint:NSMakePoint(X2, Y2)];
    [Line stroke];
}

@interface PreselView : NSView
{
    @public int Type;
    @public int Width;
    @public unsigned Color;
}
- (void)drawRect:(NSRect)rect;

@end

@implementation PreselView


- (void)drawRect:(NSRect)Rect
{
    NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];

    if (self.wantsLayer != YES) {
        self.wantsLayer = YES;
        self.layer.masksToBounds = YES;
    }

    NSRect Frame = [self bounds];
    if (Rect.size.height >= Frame.size.height) {
        NSColor *C = ColorFromHex(self->Color);
        [C set];

        if (self->Type == PRESEL_TYPE_NORTH) {
            DrawLine(self->Width, 0, 0, 0, Frame.size.height);
            DrawLine(self->Width, 0, Frame.size.height, Frame.size.width, Frame.size.height);
            DrawLine(self->Width, Frame.size.width, 0, Frame.size.width, Frame.size.height);
        } else if (self->Type == PRESEL_TYPE_EAST) {
            DrawLine(self->Width, 0, 0, Frame.size.width, 0);
            DrawLine(self->Width, Frame.size.width, 0, Frame.size.width, Frame.size.height);
            DrawLine(self->Width, 0, Frame.size.height, Frame.size.width, Frame.size.height);
        } else if (self->Type == PRESEL_TYPE_SOUTH) {
            DrawLine(self->Width, 0, 0, 0, Frame.size.height);
            DrawLine(self->Width, 0, 0, Frame.size.width, 0);
            DrawLine(self->Width, Frame.size.width, 0, Frame.size.width, Frame.size.height);
        } else if (self->Type == PRESEL_TYPE_WEST) {
            DrawLine(self->Width, 0, 0, Frame.size.width, 0);
            DrawLine(self->Width, 0, 0, 0, Frame.size.height);
            DrawLine(self->Width, 0, Frame.size.height, Frame.size.width, Frame.size.height);
        }
    }

    [Pool release];
}

@end

struct presel_window_internal
{
    int Type;
    int Width;
    unsigned Color;

    NSWindow *Handle;
    PreselView *View;
};

internal inline int
FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(int Y, int Height)
{
    CFStringRef DisplayRef = AXLibGetDisplayIdentifierForMainDisplay();
    ASSERT(DisplayRef);

    CGRect DisplayBounds = AXLibGetDisplayBounds(DisplayRef);
    CFRelease(DisplayRef);

    return DisplayBounds.size.height - (Y + Height);
}

static void
InitPreselWindow(presel_window_internal *Window, int X, int Y, int W, int H)
{
    int InvertY = FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(Y, H);
    NSRect GraphicsRect = NSMakeRect(X, InvertY, W, H);
    Window->Handle = [[NSWindow alloc] initWithContentRect: GraphicsRect
                                       styleMask: NSWindowStyleMaskBorderless
                                       backing: NSBackingStoreBuffered
                                       defer: NO];
    Window->View = [[[PreselView alloc] initWithFrame:GraphicsRect] autorelease];

    Window->View->Width = Window->Width;
    Window->View->Color = Window->Color;
    Window->View->Type  = Window->Type;

    [Window->Handle setContentView:Window->View];
    [Window->Handle setIgnoresMouseEvents:YES];
    [Window->Handle setHasShadow:NO];
    [Window->Handle setOpaque:NO];
    [Window->Handle setBackgroundColor: [NSColor clearColor]];
    [Window->Handle setCollectionBehavior:NSWindowCollectionBehaviorDefault];
    [Window->Handle setAnimationBehavior:NSWindowAnimationBehaviorNone];
    [Window->Handle setLevel:NSFloatingWindowLevel];
    [Window->Handle makeKeyAndOrderFront:nil];
    [Window->Handle setReleasedWhenClosed:YES];
}

presel_window *CreatePreselWindow(int Type, int X, int Y, int W, int H, int Width, unsigned Color)
{
    presel_window_internal *Window = (presel_window_internal *) malloc(sizeof(presel_window_internal));

    Window->Width = Width;
    Window->Color = Color;
    Window->Type  = Type;

    if ([NSThread isMainThread]) {
        NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
        InitPreselWindow(Window, X, Y, W, H);
        [Pool release];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^(void)
        {
            NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
            InitPreselWindow(Window, X, Y, W, H);
            [Pool release];
        });
    }

    return (presel_window *) Window;
}

void UpdatePreselWindow(presel_window *WindowF, int X, int Y, int W, int H)
{
    presel_window_internal *Window = (presel_window_internal *) WindowF;

    if ([NSThread isMainThread]) {
        NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
        int InvertY = FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(Y, H);
        [Window->Handle setFrame:NSMakeRect(X, InvertY, W, H) display:YES animate:NO];
        [Pool release];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^(void)
        {
            NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
            int InvertY = FuckingMacOSMonitorBoundsChangingBetweenPrimaryAndMainMonitor(Y, H);
            [Window->Handle setFrame:NSMakeRect(X, InvertY, W, H) display:YES animate:NO];
            [Pool release];
        });
    }
}

void DestroyPreselWindow(presel_window *WindowF)
{
    presel_window_internal *Window = (presel_window_internal *) WindowF;

    if ([NSThread isMainThread]) {
        NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
        [Window->Handle orderOut:nil];
        [Window->Handle close];
        [Pool release];
        free(Window);
    } else {
        dispatch_async(dispatch_get_main_queue(), ^(void)
        {
            NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
            [Window->Handle orderOut:nil];
            [Window->Handle close];
            [Pool release];
            free(Window);
        });
    }
}
