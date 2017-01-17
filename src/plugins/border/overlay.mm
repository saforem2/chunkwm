#import <Cocoa/Cocoa.h>
#include "overlay.h"

static int CornerRadius = 6;
static int BorderWidth = 2;
static int Inset = BorderWidth / 2;

@interface OverlayView : NSView
{
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
        self.layer.cornerRadius = CornerRadius;
    }

    NSRect Frame = [self bounds];
    if(Rect.size.height < Frame.size.height)
        return;

    NSRect BorderRect = CGRectInset(Frame, Inset, Inset);
    NSBezierPath *Border = [NSBezierPath bezierPathWithRoundedRect:BorderRect xRadius:CornerRadius yRadius:CornerRadius];
    [Border setLineWidth:BorderWidth];
    [[NSColor redColor] set];
    [Border stroke];
}
@end

NSWindow *BorderWindow = nil;
NSView *BorderView  = nil;
NSRect GraphicsRect;

void CreateBorder(int X, int Y, int W, int H)
{
    NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];

    GraphicsRect = NSMakeRect(X, Y, W, H);
    BorderWindow = [[NSWindow alloc]
           initWithContentRect: GraphicsRect
           styleMask: NSFullSizeContentViewWindowMask
           backing: NSBackingStoreBuffered
           defer: NO];

    BorderView = [[[OverlayView alloc] initWithFrame:GraphicsRect] autorelease];
    [BorderWindow setContentView:BorderView];
    [BorderWindow setIgnoresMouseEvents:YES];
    [BorderWindow setHasShadow:NO];
    [BorderWindow setOpaque:NO];
    [BorderWindow setBackgroundColor: [NSColor clearColor]];
    [BorderWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces];
    [BorderWindow setLevel:NSFloatingWindowLevel];
    [BorderWindow makeKeyAndOrderFront: nil];

    [Pool release];
}

static int
InvertY(int Y, int Height)
{
    static NSScreen *MainScreen = [NSScreen mainScreen];
    static NSRect Rect = [MainScreen frame];
    int InvertedY = Rect.size.height - (Y + Height);
    return InvertedY;
}
void UpdateBorder(int X, int Y, int W, int H)
{
    [BorderWindow setFrame:NSMakeRect(X - Inset, InvertY(Y + Inset, H), W + (2 * Inset), H + (2 * Inset)) display:YES animate:NO];
}
