#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mach/mach_time.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/display.h"
#include "../../common/dispatch/cgeventtap.h"
#include "../../common/misc/assert.h"

#define internal static
#define local_persist static
#define CLOCK_PRECISION 1E-9

internal event_tap EventTap;
internal bool volatile IsActive;
internal double MouseMovedDeltaTime;
internal double MouseMovedThrottle = 0.125;

internal inline void
ClockGetTime(long long *Time)
{
    local_persist mach_timebase_info_data_t Timebase;
    if(Timebase.denom == 0)
    {
        mach_timebase_info(&Timebase);
    }

    uint64_t Temp = mach_absolute_time();
    *Time = (Temp * Timebase.numer) / Timebase.denom;
}

internal inline double
GetTimeDiff(long long A, long long B)
{
    return (A - B) * CLOCK_PRECISION;
}

internal bool
IsPointInsideRect(CGPoint *Point, CGRect *Rect)
{
    if(Point->x >= Rect->origin.x &&
       Point->x <= Rect->origin.x + Rect->size.width &&
       Point->y >= Rect->origin.y &&
       Point->y <= Rect->origin.y + Rect->size.height)
        return true;

    return false;
}

#define CONTEXT_MENU_LAYER 101
struct window_info
{
    uint32_t ID;
    uint32_t PID;
    uint32_t Layer;
};

window_info GetWindowBelowCursor(CGPoint Cursor)
{
    local_persist CGWindowListOption WindowListOption = kCGWindowListOptionOnScreenOnly |
                                                        kCGWindowListExcludeDesktopElements;
    window_info Result = {};
    CFArrayRef WindowList = CGWindowListCopyWindowInfo(WindowListOption, kCGNullWindowID);
    if(WindowList)
    {
        CFIndex WindowCount = CFArrayGetCount(WindowList);
        for(size_t Index = 0; Index < WindowCount; ++Index)
        {
            CGRect WindowRect = {};
            uint32_t WindowID, WindowLayer, WindowPID;
            CFDictionaryRef Elem = (CFDictionaryRef)CFArrayGetValueAtIndex(WindowList, Index);
            CFDictionaryRef CFWindowBounds = (CFDictionaryRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowBounds"));
            CFNumberRef CFWindowNumber = (CFNumberRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowNumber"));
            CFNumberRef CFWindowPID = (CFNumberRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowOwnerPID"));
            CFNumberRef CFWindowLayer = (CFNumberRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowLayer"));
            CFNumberGetValue(CFWindowNumber, kCFNumberSInt32Type, &WindowID);
            CFNumberGetValue(CFWindowPID, kCFNumberIntType, &WindowPID);
            CFNumberGetValue(CFWindowLayer, kCFNumberSInt32Type, &WindowLayer);
            CFRelease(CFWindowNumber);
            CFRelease(CFWindowPID);
            CFRelease(CFWindowLayer);
            if(CFWindowBounds)
            {
                CGRectMakeWithDictionaryRepresentation(CFWindowBounds, &WindowRect);
            }

            CFStringRef CFOwner = (CFStringRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowOwnerName"));
            CFStringRef CFName = (CFStringRef) CFDictionaryGetValue(Elem, CFSTR("kCGWindowName"));

            bool IsOwnedByChunkWM = (CFOwner && CFStringCompare(CFOwner, CFSTR("chunkwm"), 0) == kCFCompareEqualTo);
            bool IsDock = (CFOwner && CFStringCompare(CFOwner, CFSTR("Dock"), 0) == kCFCompareEqualTo);
            bool IsLaunchpad = (CFName && CFStringCompare(CFName, CFSTR("LPSpringboard"), 0) == kCFCompareEqualTo);
            bool IsDockBar = (CFName && CFStringCompare(CFName, CFSTR("Dock"), 0) == kCFCompareEqualTo);

            if((IsDock && IsDockBar) || IsOwnedByChunkWM)
                continue;

            if((IsDock && IsLaunchpad) || (WindowLayer == CONTEXT_MENU_LAYER))
            {
                CFRelease(WindowList);
                return Result;
            }

            if(IsPointInsideRect(&Cursor, &WindowRect))
            {
                Result = (window_info) { WindowID, WindowPID, WindowLayer };
                break;
            }
        }
        CFRelease(WindowList);
    }

    return Result;
}

void FocusWindowBelowCursor(CGPoint Cursor)
{
    AXUIElementRef ApplicationRef = AXLibGetFocusedApplication();
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(ApplicationRef);
    CFRelease(ApplicationRef);

    uint32_t FocusedWindowId = 0;
    if(WindowRef)
    {
        FocusedWindowId = AXLibGetWindowID(WindowRef);
        CGPoint Position = AXLibGetWindowPosition(WindowRef);
        CGSize Size = AXLibGetWindowSize(WindowRef);
        CFRelease(WindowRef);
        CGRect WindowRect = { Position, Size };
        if(IsPointInsideRect(&Cursor, &WindowRect))
        {
            return;
        }
    }

    window_info Window = GetWindowBelowCursor(Cursor);
    if((Window.ID == FocusedWindowId) ||
       (Window.Layer != 0) ||
       (Window.ID == 0))
    {
        return;
    }

    ApplicationRef = AXUIElementCreateApplication(Window.PID);
    if(!ApplicationRef)
    {
        return;
    }

    CFArrayRef WindowList = NULL;
    AXUIElementCopyAttributeValue(ApplicationRef, kAXWindowsAttribute, (CFTypeRef*)&WindowList);
    if(!WindowList)
    {
        CFRelease(ApplicationRef);
        return;
    }

    CFIndex WindowCount = CFArrayGetCount(WindowList);
    for(CFIndex Index = 0;
        Index < WindowCount;
        ++Index)
    {
        AXUIElementRef WindowRef = (AXUIElementRef) CFArrayGetValueAtIndex(WindowList, Index);
        if(WindowRef)
        {
            int WindowRefWID = AXLibGetWindowID(WindowRef);
            if(WindowRefWID == Window.ID && !AXLibIsWindowMinimized(WindowRef))
            {
                AXLibSetFocusedWindow(WindowRef);
                AXLibSetFocusedApplication(Window.PID);
                break;
            }
        }
    }

    CFRelease(WindowList);
    CFRelease(ApplicationRef);
}

EVENTTAP_CALLBACK(EventTapCallback)
{
    event_tap *EventTap = (event_tap *) Reference;
    switch(Type)
    {
        case kCGEventTapDisabledByTimeout:
        case kCGEventTapDisabledByUserInput:
        {
            CGEventTapEnable(EventTap->Handle, true);
        } break;
        case kCGEventMouseMoved:
        {
            if(IsActive)
            {
                long long CurrentTime;
                ClockGetTime(&CurrentTime);

                if(GetTimeDiff(CurrentTime, MouseMovedDeltaTime) > MouseMovedThrottle)
                {
                    MouseMovedDeltaTime = CurrentTime;
                    CGEventFlags Flags = CGEventGetFlags(Event);
                    if(!(Flags & Event_Mask_Alt))
                    {
                        macos_space *Space;
                        bool Success = AXLibActiveSpace(&Space);
                        ASSERT(Success);

                        CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromSpace(Space->Id);
                        ASSERT(DisplayRef);
                        AXLibDestroySpace(Space);

                        if(!AXLibIsDisplayChangingSpaces(DisplayRef))
                        {
                            CGPoint Cursor = CGEventGetLocation(Event);
                            FocusWindowBelowCursor(Cursor);
                        }

                        CFRelease(DisplayRef);
                    }
                }
            }
        } break;
        default: {} break;
    }

    return Event;
}

/*
 * NOTE(koekeishiya):
 * parameter: const char *Node
 * parameter: void *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(strcmp(Node, "Tiling_focused_window_float") == 0)
    {
        uint32_t Status = *(uint32_t *) Data;
        if(Status)
        {
            printf("focus-follows-mouse: focused window is floating.");
            IsActive = false;;
        }
        else
        {
            IsActive = true;;
        }
        return true;
    }
    return false;
}

/*
 * NOTE(koekeishiya):
 * parameter: plugin_broadcast *Broadcast
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    IsActive = true;
    EventTap.Mask = (1 << kCGEventMouseMoved);
    bool Result = BeginEventTap(&EventTap, &EventTapCallback);
    return Result;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    EndEventTap(&EventTap);
}

// NOTE(koekeishiya): Enable to manually trigger ABI mismatch
#if 0
#undef CHUNKWM_PLUGIN_API_VERSION
#define CHUNKWM_PLUGIN_API_VERSION 0
#endif

// NOTE(koekeishiya): Initialize plugin function pointers.
CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)

// NOTE(koekeishiya): Subscribe to ChunkWM events!
chunkwm_plugin_export Subscriptions[] = { };
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Focus Follows Mouse", "0.1.1")
