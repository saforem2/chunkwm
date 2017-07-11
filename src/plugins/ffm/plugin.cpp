#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/application.h"
#include "../../common/dispatch/cgeventtap.h"

extern "C" int CGSMainConnectionID(void);
extern "C" OSStatus CGSFindWindowByGeometry(int cid, int zero, int one, int zero_again, CGPoint *screen_point, CGPoint *window_coords_out, int *wid_out, int *cid_out);
extern "C" CGError CGSConnectionGetPID(const int cid, pid_t *pid);

#define internal static

internal event_tap EventTap;
internal bool volatile IsActive;
internal uint32_t volatile FocusedWindowId;

internal inline void
FocusWindow(uint32_t WindowID, pid_t WindowPID)
{
    AXUIElementRef ApplicationRef;
    CFArrayRef WindowList;
    CFIndex WindowCount;

    ApplicationRef = AXUIElementCreateApplication(WindowPID);
    if(!ApplicationRef) goto out;

    WindowList = (CFArrayRef) AXLibGetWindowProperty(ApplicationRef, kAXWindowsAttribute);
    if(!WindowList) goto app_release;

    WindowCount = CFArrayGetCount(WindowList);
    for(CFIndex Index = 0; Index < WindowCount; ++Index)
    {
        AXUIElementRef WindowRef = (AXUIElementRef) CFArrayGetValueAtIndex(WindowList, Index);
        if(!WindowRef) continue;

        int WindowRefWID = AXLibGetWindowID(WindowRef);
        if(WindowRefWID != WindowID) continue;

        if(!AXLibIsWindowMinimized(WindowRef))
        {
            AXLibSetFocusedWindow(WindowRef);
            AXLibSetFocusedApplication(WindowPID);
        }
        break;
    }

    CFRelease(WindowList);
app_release:
    CFRelease(ApplicationRef);
out:;
}

internal inline void
FocusFollowsMouse(CGEventRef Event)
{
    pid_t WindowPid = 0;
    int WindowId = 0, WindowConnection = 0;
    CGPoint WindowPosition;

    int Connection = CGSMainConnectionID();
    CGPoint CursorPosition = CGEventGetLocation(Event);
    CGSFindWindowByGeometry(Connection, 0, 1, 0, &CursorPosition, &WindowPosition, &WindowId, &WindowConnection);

    if(Connection == WindowConnection) return;
    if(WindowId == FocusedWindowId)    return;

    CGSConnectionGetPID(WindowConnection, &WindowPid);
    FocusWindow(WindowId, WindowPid);
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
            if(!IsActive) break;

            CGEventFlags Flags = CGEventGetFlags(Event);
            if(Flags & Event_Mask_Alt) break;

            FocusFollowsMouse(Event);
        } break;
        default: {} break;
    }

    return Event;
}

internal inline void
ApplicationActivatedHandler(void *Data)
{
    macos_application *Application = (macos_application *) Data;
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if(WindowRef)
    {
        uint32_t WindowId = AXLibGetWindowID(WindowRef);
        FocusedWindowId = WindowId;
        CFRelease(WindowRef);
    }
}

internal inline void
WindowFocusedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    FocusedWindowId = Window->Id;
}

internal inline void
TilingWindowFloatHandler(void *Data)
{
    uint32_t Status = *(uint32_t *) Data;
    IsActive = !(Status & 0x1);
}

PLUGIN_MAIN_FUNC(PluginMain)
{
    if(strcmp(Node, "chunkwm_export_application_activated") == 0)
    {
        ApplicationActivatedHandler(Data);
        return true;
    }
    else if(strcmp(Node, "chunkwm_export_window_focused") == 0)
    {
        WindowFocusedHandler(Data);
        return true;
    }
    else if(strcmp(Node, "Tiling_focused_window_float") == 0)
    {
        TilingWindowFloatHandler(Data);
        return true;
    }
    return false;
}

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

CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)
chunkwm_plugin_export Subscriptions[] =
{
    chunkwm_export_application_activated,
    chunkwm_export_window_focused
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)
CHUNKWM_PLUGIN("Focus Follows Mouse", "0.2.0")
