#ifndef AXLIB_DISPLAY_H
#define AXLIB_DISPLAY_H

#include <Carbon/Carbon.h>

/* NOTE(koekeishiya): User controlled spaces */
#define kCGSSpaceUser 0

/* NOTE(koekeishiya): System controlled spaces (dashboard) */
#define kCGSSpaceSystem 2

/* NOTE(koekeishiya): Fullscreen applications */
#define kCGSSpaceFullscreen 4

enum macos_dock_orientation
{
    Dock_Orientation_Top = 1,
    Dock_Orientation_Bottom = 2,
    Dock_Orientation_Left = 3,
    Dock_Orientation_Right = 4,
};

enum CGSSpaceSelector
{
    kCGSSpaceCurrent = 5,
    kCGSSpaceOther = 6,
    kCGSSpaceAll = 7
};

typedef int CGSSpaceID;
typedef int CGSSpaceType;

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
unsigned AXLibDisplayCount();

CGSSpaceID AXLibActiveCGSSpaceID(CFStringRef DisplayRef);
macos_space *AXLibActiveSpace(CFStringRef DisplayRef);
bool AXLibActiveSpace(macos_space **Space);
void AXLibDestroySpace(macos_space *Space);

CFStringRef AXLibGetDisplayIdentifier(CGDirectDisplayID Id);
CFStringRef AXLibGetDisplayIdentifierFromArrangement(unsigned Arrangement);
CFStringRef AXLibGetDisplayIdentifierFromSpace(CGSSpaceID Space);
CFStringRef AXLibGetDisplayIdentifierFromWindow(uint32_t WindowId);
CFStringRef AXLibGetDisplayIdentifierFromWindowRect(CGPoint Position, CGSize Size);
CFStringRef AXLibGetDisplayIdentifierForMainDisplay();
CFStringRef AXLibGetDisplayIdentifierForRightMostDisplay();
CFStringRef AXLibGetDisplayIdentifierForLeftMostDisplay();
CFStringRef AXLibGetDisplayIdentifierForBottomMostDisplay();

/*
 * NOTE(koekeishiya): Memory-ownership differs on El Capitan and newer versions.
 * On El Capitan we do not want to free the DisplayRef if it was retrieved through
 * the private CGSCopyManagedDisplayForWindow function
 *
 *      CFStringRef AXLibGetDisplayIdentifierFromWindow(uint32_t WindowId);
 */

#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
#define __AppleGetDisplayIdentifierFromMacOSWindowO(Window, DisplayRef) \
            bool ShouldFreeDisplayRef = false; \
            DisplayRef = AXLibGetDisplayIdentifierFromWindow(Window->Id); \
            if (!DisplayRef) { \
                DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size); \
                ShouldFreeDisplayRef = true; \
            }
#define __AppleGetDisplayIdentifierFromMacOSWindow(Window) \
            bool ShouldFreeDisplayRef = false; \
            CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindow(Window->Id); \
            if (!DisplayRef) { \
                DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size); \
                ShouldFreeDisplayRef = true; \
            }
#define __AppleGetDisplayIdentifierFromWindow(WindowRef, WindowId) \
            bool ShouldFreeDisplayRef = false; \
            CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindow(WindowId); \
            if (!DisplayRef) { \
                CGPoint Position = AXLibGetWindowPosition(WindowRef); \
                CGSize Size = AXLibGetWindowSize(WindowRef); \
                DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Position, Size); \
                ShouldFreeDisplayRef = true; \
            }
#define __AppleFreeDisplayIdentifierFromWindowO(DisplayRef) \
            if (ShouldFreeDisplayRef) { \
                CFRelease(DisplayRef); \
            }
#define __AppleFreeDisplayIdentifierFromWindow() \
            if (ShouldFreeDisplayRef) { \
                CFRelease(DisplayRef); \
            }
#else
#define __AppleGetDisplayIdentifierFromMacOSWindowO(Window, DisplayRef) \
            DisplayRef = AXLibGetDisplayIdentifierFromWindow(Window->Id); \
            if (!DisplayRef) { \
                DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size); \
            }
#define __AppleGetDisplayIdentifierFromMacOSWindow(Window) \
            CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindow(Window->Id); \
            if (!DisplayRef) { \
                DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size); \
            }
#define __AppleGetDisplayIdentifierFromWindow(WindowRef, WindowId) \
            CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindow(WindowId); \
            if (!DisplayRef) { \
                CGPoint Position = AXLibGetWindowPosition(WindowRef); \
                CGSize Size = AXLibGetWindowSize(WindowRef); \
                DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Position, Size); \
            }
#define __AppleFreeDisplayIdentifierFromWindowO(DisplayRef) \
            CFRelease(DisplayRef);
#define __AppleFreeDisplayIdentifierFromWindow() \
            CFRelease(DisplayRef);
#endif


CGRect AXLibGetDisplayBounds(CFStringRef DisplayRef);
bool AXLibIsDisplayChangingSpaces(CFStringRef DisplayRef);

bool AXLibCGSSpaceIDToDesktopID(CGSSpaceID SpaceId, unsigned *OutArrangement, unsigned *OutDesktopId);
bool AXLibCGSSpaceIDFromDesktopID(unsigned DesktopId, unsigned *OutArrangement, CGSSpaceID *OutSpaceId);

int *AXLibSpacesForDisplay(CFStringRef DisplayRef, int *Count);
macos_space **AXLibSpacesForDisplay(CFStringRef DisplayRef);
macos_space **AXLibSpacesForWindow(uint32_t WindowId);
void AXLibSpaceMoveWindow(CGSSpaceID SpaceId, uint32_t WindowId);
void AXLibSpaceAddWindow(CGSSpaceID SpaceId, uint32_t WindowId);
void AXLibSpaceRemoveWindow(CGSSpaceID SpaceId, uint32_t WindowId);
bool AXLibSpaceHasWindow(CGSSpaceID SpaceId, uint32_t WindowId);
bool AXLibStickyWindow(uint32_t WindowId);

bool AXLibIsMenuBarAutoHideEnabled();
bool AXLibIsDockAutoHideEnabled();

macos_dock_orientation AXLibGetDockOrientation();
size_t AXLibGetDockTileSize();

bool AXLibDisplayHasSeparateSpaces();

#endif
