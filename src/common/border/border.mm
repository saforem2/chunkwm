#import <Cocoa/Cocoa.h>
#include "border.h"

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
    if(self.wantsLayer != YES)
    {
        self.wantsLayer = YES;
        self.layer.masksToBounds = YES;
        self.layer.cornerRadius = self->Radius;
    }

    NSRect Frame = [self bounds];
    if(Rect.size.height < Frame.size.height)
        return;

    NSBezierPath *Border = [NSBezierPath bezierPathWithRoundedRect:Frame xRadius:self->Radius yRadius:self->Radius];
    [Border setLineWidth:self->Width];
    NSColor *Color = ColorFromHex(self->Color);
    [Color set];
    [Border stroke];
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

static int
InvertY(int Y, int Height)
{
    static NSScreen *MainScreen = [NSScreen mainScreen];
    static NSRect Rect = [MainScreen frame];
    int InvertedY = Rect.size.height - (Y + Height);
    return InvertedY;
}

border_window *CreateBorderWindow(int X, int Y, int W, int H, int BorderWidth, int BorderRadius, unsigned int BorderColor)
{
    border_window_internal *Border = (border_window_internal *) malloc(sizeof(border_window_internal));

    Border->Width = BorderWidth;
    Border->Radius = BorderRadius;
    Border->Color = BorderColor;

    dispatch_sync(dispatch_get_main_queue(), ^(void)
    {
        NSRect GraphicsRect = NSMakeRect(X, InvertY(Y, H), W, H);
        Border->Handle = [[NSWindow alloc] initWithContentRect: GraphicsRect
                                           styleMask: NSWindowStyleMaskFullSizeContentView
                                           backing: NSBackingStoreBuffered
                                           defer: NO];
        Border->View = [[OverlayView alloc] initWithFrame:GraphicsRect];

        Border->View->Width = Border->Width;
        Border->View->Radius = Border->Radius;
        Border->View->Color = Border->Color;

        [Border->Handle setContentView:Border->View];
        [Border->Handle setIgnoresMouseEvents:YES];
        [Border->Handle setHasShadow:NO];
        [Border->Handle setOpaque:NO];
        [Border->Handle setBackgroundColor: [NSColor clearColor]];
        [Border->Handle setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces];
        [Border->Handle setLevel:NSFloatingWindowLevel];
        [Border->Handle makeKeyAndOrderFront:nil];
        [Border->Handle setReleasedWhenClosed:YES];
    });

    return (border_window *) Border;
}

void UpdateBorderWindowRect(border_window *Border, int X, int Y, int W, int H)
{
    border_window_internal *BorderInternal = (border_window_internal *) Border;
    dispatch_async(dispatch_get_main_queue(), ^(void)
    {
        [BorderInternal->Handle setFrame:NSMakeRect(X, InvertY(Y, H), W, H) display:YES animate:NO];
    });
}

void UpdateBorderWindowColor(border_window *Border, unsigned Color)
{
    border_window_internal *BorderInternal = (border_window_internal *) Border;
    BorderInternal->Color = Color;
    BorderInternal->View->Color = BorderInternal->Color;

    dispatch_async(dispatch_get_main_queue(), ^(void)
    {
        [BorderInternal->View setNeedsDisplay:YES];
    });
}

void DestroyBorderWindow(border_window *Border)
{
    border_window_internal *BorderInternal = (border_window_internal *) Border;
    if(BorderInternal->Handle)
    {
        dispatch_sync(dispatch_get_main_queue(), ^(void)
        {
            [BorderInternal->Handle close];
            [BorderInternal->View release];
        });
        BorderInternal->Handle = nil;
        BorderInternal->View = nil;
    }
    free(BorderInternal);
}
