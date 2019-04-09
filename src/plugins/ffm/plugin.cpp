#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/application.h"
#include "../../common/config/cvar.h"
#include "../../common/config/tokenize.h"
#include "../../common/dispatch/cgeventtap.h"

#include "../../common/accessibility/element.cpp"
#include "../../common/config/cvar.cpp"
#include "../../common/config/tokenize.cpp"
#include "../../common/dispatch/cgeventtap.cpp"

#define ArrayCount(a) (sizeof(a) / sizeof((*a)))
#define internal static
#define local_persist static

#define kCPSAllWindows    0x100
#define kCPSUserGenerated 0x200
#define kCPSNoWindows     0x400

extern "C" int CGSMainConnectionID(void);
extern "C" CGError CGSGetWindowLevel(const int cid, int wid, int *wlvl);
extern "C" OSStatus CGSFindWindowByGeometry(int cid, int zero, int one, int zero_again, CGPoint *screen_point, CGPoint *window_coords_out, int *wid_out, int *cid_out);
extern "C" CGError CGSConnectionGetPID(const int cid, pid_t *pid);
extern "C" CGError _SLPSSetFrontProcessWithOptions(ProcessSerialNumber *psn, unsigned int wid, unsigned int mode);
extern "C" CGError SLPSPostEventRecordTo(ProcessSerialNumber *psn, uint8_t *bytes);

internal event_tap EventTap;
internal uint32_t MouseModifier;
internal bool volatile IsActive;
internal int Connection;
internal uint32_t volatile FocusedWindowId;
internal pid_t volatile FocusedWindowPid;
internal ProcessSerialNumber volatile FocusedWindowPsn;
internal chunkwm_api API;
internal AXUIElementRef SystemWideElement;
internal float MouseMotionInterval;
internal float LastEventTime;
internal bool StandbyOnFloat;
internal bool DisableAutoraise;

internal bool
IsWindowLevelAllowed(int WindowLevel)
{
    local_persist int ValidWindowLevels[] = {
        CGWindowLevelForKey(kCGNormalWindowLevelKey),
        CGWindowLevelForKey(kCGFloatingWindowLevelKey),
        CGWindowLevelForKey(kCGModalPanelWindowLevelKey),
    };
    local_persist int Count = ArrayCount(ValidWindowLevels);

    for (int Index = 0; Index < Count; ++Index) {
        if (WindowLevel == ValidWindowLevels[Index]) {
            return true;
        }
    }

    return false;
}

internal void
send_event(ProcessSerialNumber *WindowPsn, uint32_t WindowId, uint8_t op)
{
    uint8_t bytes[0xf8] = {
        [0x4] = 0xf8,
        [0x8] = 0xd,
        [0x8a] = op
    };
    memcpy(bytes + 0x3c, &WindowId, sizeof(uint32_t));
    SLPSPostEventRecordTo(WindowPsn, bytes);
}

internal void
send_defer_ordering_event(ProcessSerialNumber *psn, uint32_t wid)
{
    send_event(psn, wid, 0x9);
}

internal void
send_pseudo_activate_event(ProcessSerialNumber *psn)
{
    send_event(psn, 0, 0x2);
}

internal void
send_reactivate_event(ProcessSerialNumber *psn, uint32_t wid)
{
    send_event(psn, wid, 0x12);
}

internal inline void
FocusFollowsMouse(CGEventRef Event)
{
    int WindowId = 0;
    int WindowLevel = 0;
    pid_t WindowPid = 0;
    int WindowConnection = 0;
    CGPoint WindowPosition;
    CFStringRef Role;
    AXUIElementRef Element;
    AXUIElementRef WindowRef = NULL;
    ProcessSerialNumber WindowPsn;

    CGPoint CursorPosition = CGEventGetLocation(Event);
    CGSFindWindowByGeometry(Connection, 0, 1, 0, &CursorPosition, &WindowPosition, &WindowId, &WindowConnection);

    if (WindowId == 0)                  return;
    if (Connection == WindowConnection) return;
    if (WindowId == FocusedWindowId)    return;

    CGSGetWindowLevel(Connection, WindowId, &WindowLevel);
    if (!IsWindowLevelAllowed(WindowLevel)) return;
    CGSConnectionGetPID(WindowConnection, &WindowPid);
    GetProcessForPID(WindowPid, &WindowPsn);

    AXUIElementCopyElementAtPosition(SystemWideElement, CursorPosition.x, CursorPosition.y, &Element);
    if (!Element) return;

    if (AXLibGetWindowRole(Element, &Role)) {
        if (CFEqual(Role, kAXWindowRole)) {
            WindowRef = Element;
        } else {
            AXUIElementCopyAttributeValue(Element, kAXWindowAttribute, (CFTypeRef*)&WindowRef);
            CFRelease(Element);
        }
        CFRelease(Role);
    }
    if (!WindowRef) return;

    if (DisableAutoraise) {
        _SLPSSetFrontProcessWithOptions(&WindowPsn, WindowId, kCPSUserGenerated);
        send_reactivate_event(&WindowPsn, WindowId);

#if 0
        if (FocusedWindowPid == WindowPid) {
        } else {
        }
#endif
    } else {
        AXLibSetFocusedWindow(WindowRef);
        AXLibSetFocusedApplication(WindowPid);
    }

    CFRelease(WindowRef);
}

internal bool
ShouldProcessEvent(CGEventRef Event)
{
    uint64_t CurrentEventTime = CGEventGetTimestamp(Event);
    float DeltaEventTime = ((float)CurrentEventTime  - LastEventTime) * (1.0f / 1E6);
    if (DeltaEventTime < MouseMotionInterval) return false;
    LastEventTime = CurrentEventTime;
    return true;
}

EVENTTAP_CALLBACK(EventTapCallback)
{
    event_tap *EventTap = (event_tap *) Reference;
    switch (Type) {
    case kCGEventTapDisabledByTimeout:
    case kCGEventTapDisabledByUserInput: {
        CGEventTapEnable(EventTap->Handle, true);
    } break;
    case kCGEventMouseMoved: {
        if (!ShouldProcessEvent(Event)) break;
        if (StandbyOnFloat && !IsActive) break;

        CGEventFlags Flags = CGEventGetFlags(Event);
        if ((Flags & MouseModifier) == MouseModifier) break;

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
    if (WindowRef) {
        uint32_t WindowId = AXLibGetWindowID(WindowRef);
        FocusedWindowId = WindowId;
        FocusedWindowPid = Application->PID;
        memcpy((void*)&FocusedWindowPsn, (void*)&Application->PSN, sizeof(ProcessSerialNumber));
        CFRelease(WindowRef);
    }
}

internal inline void
WindowFocusedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    FocusedWindowId = Window->Id;
    FocusedWindowPid = Window->Owner->PID;
    memcpy((void*)&FocusedWindowPsn, (void*)&Window->Owner->PSN, sizeof(ProcessSerialNumber));
}

internal inline void
TilingWindowFloatHandler(void *Data)
{
    uint32_t WindowId = *(uint32_t *) Data;
    uint32_t Status = *((uint32_t *) Data + 1);
    IsActive = !(Status & 0x1);
}

internal inline void
SetMouseModifier(const char *Mod)
{
    while (Mod && *Mod) {
        token ModToken = GetToken(&Mod);
        if (TokenEquals(ModToken, "fn")) {
            MouseModifier |= Event_Mask_Fn;
        } else if (TokenEquals(ModToken, "shift")) {
            MouseModifier |= Event_Mask_Shift;
        } else if (TokenEquals(ModToken, "alt")) {
            MouseModifier |= Event_Mask_Alt;
        } else if (TokenEquals(ModToken, "cmd")) {
            MouseModifier |= Event_Mask_Cmd;
        } else if (TokenEquals(ModToken, "ctrl")) {
            MouseModifier |= Event_Mask_Control;
        }
    }

    // NOTE(koekeishiya): If no matches were found, we default to FN
    if (MouseModifier == 0) MouseModifier |= Event_Mask_Fn;
}

PLUGIN_MAIN_FUNC(PluginMain)
{
    if (strcmp(Node, "chunkwm_export_application_activated") == 0) {
        ApplicationActivatedHandler(Data);
        return true;
    } else if (strcmp(Node, "chunkwm_export_window_focused") == 0) {
        WindowFocusedHandler(Data);
        return true;
    } else if (strcmp(Node, "Tiling_focused_window_float") == 0) {
        TilingWindowFloatHandler(Data);
        return true;
    }
    return false;
}

PLUGIN_BOOL_FUNC(PluginInit)
{
    API = ChunkwmAPI;
    SystemWideElement = AXUIElementCreateSystemWide();
    if (!SystemWideElement) return false;

    IsActive = true;
    EventTap.Mask = (1 << kCGEventMouseMoved);
    bool Result = BeginEventTap(&EventTap, &EventTapCallback);
    if (Result) {
        Connection = CGSMainConnectionID();
        BeginCVars(&API);
        CreateCVar("ffm_bypass_modifier", "fn");
        CreateCVar("ffm_standby_on_float", 1);
        CreateCVar("ffm_disable_autoraise", 0);
        CreateCVar("mouse_motion_interval", 35.0f);
        SetMouseModifier(CVarStringValue("ffm_bypass_modifier"));
        DisableAutoraise = CVarIntegerValue("ffm_disable_autoraise");
        StandbyOnFloat = CVarIntegerValue("ffm_standby_on_float");
        MouseMotionInterval = CVarFloatingPointValue("mouse_motion_interval");
    }
    return Result;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    EndEventTap(&EventTap);
    CFRelease(SystemWideElement);
}

CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)
chunkwm_plugin_export Subscriptions[] =
{
    chunkwm_export_application_activated,
    chunkwm_export_window_focused
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)
CHUNKWM_PLUGIN("Focus Follows Mouse", "0.3.5")
