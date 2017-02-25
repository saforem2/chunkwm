#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/element.h"
#include "../../common/dispatch/cgeventtap.h"

#define internal static
internal event_tap EventTap;
internal bool volatile IsActive;

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
};

window_info GetWindowBelowCursor()
{
    static CGWindowListOption WindowListOption = kCGWindowListOptionOnScreenOnly |
                                                 kCGWindowListExcludeDesktopElements;
    window_info Result = {};
    CGPoint Cursor = AXLibGetCursorPos();

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
                Result = (window_info) { WindowID, WindowPID };
                break;
            }
        }

        CFRelease(WindowList);
    }

    return Result;
}

void FocusWindowBelowCursor()
{
    AXUIElementRef ApplicationRef = AXLibGetFocusedApplication();
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(ApplicationRef);
    CFRelease(ApplicationRef);

    uint32_t FocusedWindowId = 0;
    if(WindowRef)
    {
        FocusedWindowId = AXLibGetWindowID(WindowRef);
        CFRelease(WindowRef);
    }

    window_info Window = GetWindowBelowCursor();
    if(Window.ID == 0 || Window.ID == FocusedWindowId)
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

enum osx_event_mask
{
    Event_Mask_Alt = 0x00080000,
    Event_Mask_LAlt = 0x00000020,
    Event_Mask_RAlt = 0x00000040,

    Event_Mask_Shift = 0x00020000,
    Event_Mask_LShift = 0x00000002,
    Event_Mask_RShift = 0x00000004,

    Event_Mask_Cmd = 0x00100000,
    Event_Mask_LCmd = 0x00000008,
    Event_Mask_RCmd = 0x00000010,

    Event_Mask_Control = 0x00040000,
    Event_Mask_LControl = 0x00000001,
    Event_Mask_RControl = 0x00002000,
};

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
                CGEventFlags Flags = CGEventGetFlags(Event);
                if(!(Flags & Event_Mask_Alt))
                {
                    FocusWindowBelowCursor();
                }
            }
        } break;
        default: {} break;
    }

    return Event;
}

/*
 * NOTE(koekeishiya): Function parameters
 * const char *Node
 * const char *Data
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
            printf("focus-follows-mouse: focused window is not floating.");
            IsActive = true;;
        }
        return true;
    }
    return false;
}

/*
 * NOTE(koekeishiya):
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
#undef PLUGIN_API_VERSION
#define PLUGIN_API_VERSION 0
#endif

// NOTE(koekeishiya): Initialize plugin function pointers.
CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)

// NOTE(koekeishiya): Subscribe to ChunkWM events!
chunkwm_plugin_export Subscriptions[] = { };
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Focus Follows Mouse", "0.0.1")
