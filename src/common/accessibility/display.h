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

CGSSpaceID AXLibActiveCGSSpaceID(CFStringRef DisplayRef);
macos_space *AXLibActiveSpace(CFStringRef DisplayRef);
bool AXLibActiveSpace(macos_space **Space);
void AXLibDestroySpace(macos_space *Space);

CFStringRef AXLibGetDisplayIdentifierFromArrangement(unsigned Arrangement);
CFStringRef AXLibGetDisplayIdentifierFromSpace(CGSSpaceID Space);

CGRect AXLibGetDisplayBounds(CFStringRef DisplayRef);
bool AXLibIsDisplayChangingSpaces(CFStringRef DisplayRef);

bool AXLibCGSSpaceIDToDesktopID(CGSSpaceID SpaceId, unsigned *OutArrangement, unsigned *OutDesktopId);
bool AXLibCGSSpaceIDFromDesktopID(unsigned DesktopId, unsigned *OutArrangement, CGSSpaceID *OutSpaceId);

bool AXLibSpaceHasWindow(CGSSpaceID SpaceId, uint32_t WindowId);
bool AXLibStickyWindow(uint32_t WindowId);

#endif
