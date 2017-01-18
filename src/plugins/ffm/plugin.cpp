#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/element.h"
#include "../../common/dispatch/cgeventtap.h"

#define internal static
internal event_tap EventTap;

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
    ax_application *Application = AXLibGetFocusedApplication();
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if(WindowRef)
    {
        CGRect WindowRect = { AXLibGetWindowPosition(WindowRef),
                              AXLibGetWindowSize(WindowRef) };
        CFRelease(WindowRef);
        CGPoint Cursor = AXLibGetCursorPos();
        if(IsPointInsideRect(&Cursor, &WindowRect))
        {
            AXLibDestroyApplication(Application);
            return;
        }
    }
    AXLibDestroyApplication(Application);

    window_info Window = GetWindowBelowCursor();
    if(Window.ID == 0)
    {
        return;
    }

    AXUIElementRef ApplicationRef = AXUIElementCreateApplication(Window.PID);
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
        AXUIElementRef WindowRef = (AXUIElementRef)CFArrayGetValueAtIndex(WindowList, Index);
        if(WindowRef)
        {
            int WindowRefWID = AXLibGetWindowID(WindowRef);
            if(WindowRefWID == Window.ID)
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
            FocusWindowBelowCursor();
        } break;
        default: {} break;
    }

    return Event;
}

/*
 * NOTE(koekeishiya): Function parameters
 * plugin *Plugin
 * const char *Node
 * const char *Data
 * unsigned int DataSize
 *
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(Node)
    {
        return true;
    }

    return false;
}

/*
 * NOTE(koekeishiya):
 * param: plugin *Plugin
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    printf("Plugin Init!\n");
    EventTap.Mask = (1 << kCGEventMouseMoved);
    bool Result = BeginEventTap(&EventTap, &EventTapCallback);
    return Result;
}

/*
 * NOTE(koekeishiya):
 * param: plugin *Plugin
 * return: void
 */
PLUGIN_VOID_FUNC(PluginDeInit)
{
    printf("Plugin DeInit!\n");
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
