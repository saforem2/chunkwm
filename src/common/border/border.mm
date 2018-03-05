#import <Cocoa/Cocoa.h>
#include "border.h"

#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
#define NSWindowStyleMaskBorderless NSBorderlessWindowMask
#endif

NSColor *ColorFromHex(unsigned int Color);

@interface OverlayView : NSView
{
    @public int Width;
    @public int Radius;
    @public unsigned Color;
}
- (void)drawRect:(NSRect)rect;
@end

@implementation OverlayView

- (void)drawRect:(NSRect)Rect
{
    NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];

    if (self.wantsLayer != YES) {
        self.wantsLayer = YES;
        self.layer.masksToBounds = YES;
        self.layer.cornerRadius = self->Radius;
    }

    NSRect Frame = [self bounds];
    if (Rect.size.height >= Frame.size.height) {
        NSBezierPath *Border = [NSBezierPath bezierPathWithRoundedRect:Frame xRadius:self->Radius yRadius:self->Radius];
        [Border setLineWidth:self->Width];
        NSColor *C = ColorFromHex(self->Color);
        [C set];
        [Border stroke];
    }

    [Pool release];
}

@end

struct border_window_internal
{
    int Width;
    int Radius;
    unsigned Color;

    NSWindow *Handle;
    OverlayView *View;
};

NSColor *ColorFromHex(unsigned int Color)
{
    float Red = ((Color >> 16) & 0xff) / 255.0;
    float Green = ((Color >> 8) & 0xff) / 255.0;
    float Blue = ((Color >> 0) & 0xff) / 255.0;
    float Alpha = ((Color >> 24) & 0xff) / 255.0;
    return [NSColor colorWithCalibratedRed:(Red) green:(Green) blue:(Blue) alpha:Alpha];
}

static void
InitBorderWindow(border_window_internal *Border, int X, int Y, int W, int H, int BorderWidth, int BorderRadius, unsigned int BorderColor)
{
    NSRect GraphicsRect = NSMakeRect(X, Y, W, H);
    Border->Handle = [[NSWindow alloc] initWithContentRect: GraphicsRect
                                       styleMask: NSWindowStyleMaskBorderless
                                       backing: NSBackingStoreBuffered
                                       defer: NO];
    Border->View = [[[OverlayView alloc] initWithFrame:GraphicsRect] autorelease];

    Border->View->Width = Border->Width;
    Border->View->Radius = Border->Radius;
    Border->View->Color = Border->Color;

    [Border->Handle setContentView:Border->View];
    [Border->Handle setIgnoresMouseEvents:YES];
    [Border->Handle setHasShadow:NO];
    [Border->Handle setOpaque:NO];
    [Border->Handle setBackgroundColor: [NSColor clearColor]];
    [Border->Handle setCollectionBehavior:NSWindowCollectionBehaviorDefault];
    [Border->Handle setAnimationBehavior:NSWindowAnimationBehaviorNone];
    [Border->Handle setLevel:NSFloatingWindowLevel];
    [Border->Handle makeKeyAndOrderFront:nil];
    [Border->Handle setReleasedWhenClosed:YES];
}

border_window *CreateBorderWindow(int X, int Y, int W, int H, int BorderWidth, int BorderRadius, unsigned int BorderColor)
{
    border_window_internal *Border = (border_window_internal *) malloc(sizeof(border_window_internal));

    Border->Width = BorderWidth;
    Border->Radius = BorderRadius;
    Border->Color = BorderColor;

    if ([NSThread isMainThread]) {
        NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
        InitBorderWindow(Border, X, Y, W, H, BorderWidth, BorderRadius, BorderColor);
        [Pool release];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^(void)
        {
            NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
            InitBorderWindow(Border, X, Y, W, H, BorderWidth, BorderRadius, BorderColor);
            [Pool release];
        });
    }

    return (border_window *) Border;
}

void UpdateBorderWindowRect(border_window *Border, int X, int Y, int W, int H)
{
    border_window_internal *BorderInternal = (border_window_internal *) Border;

    if ([NSThread isMainThread]) {
        NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
        [BorderInternal->Handle setFrame:NSMakeRect(X, Y, W, H) display:YES animate:NO];
        [Pool release];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^(void)
        {
            NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
            [BorderInternal->Handle setFrame:NSMakeRect(X, Y, W, H) display:YES animate:NO];
            [Pool release];
        });
    }
}

void UpdateBorderWindowColor(border_window *Border, unsigned Color)
{
    border_window_internal *BorderInternal = (border_window_internal *) Border;
    BorderInternal->Color = Color;
    BorderInternal->View->Color = BorderInternal->Color;

    if ([NSThread isMainThread]) {
        [BorderInternal->View setNeedsDisplay:YES];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^(void)
        {
            [BorderInternal->View setNeedsDisplay:YES];
        });
    }
}

void UpdateBorderWindowWidth(border_window *Border, int BorderWidth)
{
    border_window_internal *BorderInternal = (border_window_internal *) Border;
    BorderInternal->Width = BorderWidth;
    BorderInternal->View->Width = BorderInternal->Width;

    if ([NSThread isMainThread]) {
        [BorderInternal->View setNeedsDisplay:YES];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^(void)
        {
            [BorderInternal->View setNeedsDisplay:YES];
        });
    }
}

void DestroyBorderWindow(border_window *Border)
{
    border_window_internal *BorderInternal = (border_window_internal *) Border;

    if ([NSThread isMainThread]) {
        NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
        [BorderInternal->Handle orderOut:nil];
        [BorderInternal->Handle close];
        [Pool release];
        free(BorderInternal);
    } else {
        dispatch_async(dispatch_get_main_queue(), ^(void)
        {
            NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];
            [BorderInternal->Handle orderOut:nil];
            [BorderInternal->Handle close];
            [Pool release];
            free(BorderInternal);
        });
    }
}
