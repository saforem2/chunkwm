#ifndef AXLIB_DISPLAY_H
#define AXLIB_DISPLAY_H

#include <Carbon/Carbon.h>

/* NOTE(koekeishiya): User controlled spaces */
#define kCGSSpaceUser 0

/* NOTE(koekeishiya): System controlled spaces (dashboard) */
#define kCGSSpaceSystem 2

/* NOTE(koekeishiya): Fullscreen applications */
#define kCGSSpaceFullscreen 4

enum CGSSpaceSelector
{
    kCGSSpaceCurrent = 5,
    kCGSSpaceOther = 6,
    kCGSSpaceAll = 7
};

typedef int CGSSpaceID;
typedef int CGSSpaceType;
/* NOTE(koekeishiya):
 * CGDirectDisplayID   typedef  -> uint32_t
 * */

struct macos_space
{
    CFStringRef Ref;
    CGSSpaceID Id;
    CGSSpaceType Type;
};

struct macos_display
{
    CFStringRef Ref;
    CGDirectDisplayID Id;
    unsigned Arrangement;

    float X, Y;
    float Width, Height;
};

macos_display *AXLibConstructDisplay(CGDirectDisplayID Id, unsigned Arrangement);
void  AXLibDestroyDisplay(macos_display *Display);
macos_display **AXLibDisplayList(unsigned *Count);

macos_space *AXLibActiveSpace(CFStringRef DisplayRef);
void AXLibDestroySpace(macos_space *Space);

unsigned AXLibCGSSpaceIDToDesktopID(CFStringRef DisplayRef, CGSSpaceID SpaceId);
CGSSpaceID AXLibCGSSpaceIDFromDesktopID(CFStringRef DisplayRef, unsigned DesktopId);

bool AXLibSpaceHasWindow(CGSSpaceID SpaceId, uint32_t WindowId);
bool AXLibStickyWindow(uint32_t WindowId);

#endif
