#import <Cocoa/Cocoa.h>
#include "overlay.h"

static bool BorderCreated = false;
static int CornerRadius = 4;
static int BorderWidth = 4;
unsigned int HexColor = 0xddd5c4a1;

NSColor *ColorFromHex(unsigned int Color)
{
    float Red = ((Color >> 16) & 0xff) / 255.0;
    float Green = ((Color >> 8) & 0xff) / 255.0;
    float Blue = ((Color >> 0) & 0xff) / 255.0;
    float Alpha = ((Color >> 24) & 0xff) / 255.0;
    return [NSColor colorWithCalibratedRed:(Red) green:(Green) blue:(Blue) alpha:Alpha];
}

NSColor *Color = ColorFromHex(HexColor);

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

    NSBezierPath *Border = [NSBezierPath bezierPathWithRoundedRect:Frame xRadius:CornerRadius yRadius:CornerRadius];
    [Border setLineWidth:BorderWidth];
    [Color set];
    [Border stroke];
}
@end

NSWindow *BorderWindow = nil;
NSView *BorderView  = nil;

static int
InvertY(int Y, int Height)
{
    static NSScreen *MainScreen = [NSScreen mainScreen];
    static NSRect Rect = [MainScreen frame];
    int InvertedY = Rect.size.height - (Y + Height);
    return InvertedY;
}

void CreateBorder(int X, int Y, int W, int H)
{
    NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];

    NSRect GraphicsRect = NSMakeRect(X, InvertY(Y, H), W, H);
    BorderWindow = [[NSWindow alloc]
           initWithContentRect: GraphicsRect
           styleMask: NSWindowStyleMaskFullSizeContentView
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
    [BorderWindow makeKeyAndOrderFront:nil];
    [BorderWindow setReleasedWhenClosed:YES];

    [Pool release];
}

void UpdateBorder(int X, int Y, int W, int H)
{
    if(BorderCreated)
    {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void)
        {
            dispatch_async(dispatch_get_main_queue(), ^(void)
            {
                [BorderWindow setFrame:NSMakeRect(X, InvertY(Y, H), W, H) display:YES animate:NO];
            });
        });
    }
    else
    {
        BorderCreated = true;
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void)
        {
            dispatch_async(dispatch_get_main_queue(), ^(void)
            {
                CreateBorder(X, Y, W, H);
            });
        });
    }
}

void DestroyBorder()
{
    if(BorderWindow)
    {
        [BorderWindow close];
        BorderWindow = nil;
        BorderView = nil;
        BorderCreated = false;
    }
}
