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
#include "../../common/accessibility/window.cpp"
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
extern "C" OSStatus CGSFindWindowByGeometry(int cid, int zero, int one, int zero_again, CGPoint *screen_point, CGPoint *window_coords_out, int *wid_out, int *cid_out);
extern "C" CGError CGSConnectionGetPID(const int cid, pid_t *pid);
extern "C" CGError _SLPSSetFrontProcessWithOptions(ProcessSerialNumber *psn, unsigned int wid, unsigned int mode);
extern "C" CGError SLPSPostEventRecordTo(ProcessSerialNumber *psn, uint8_t *bytes);

internal event_tap EventTap;
internal uint32_t MouseModifier;
internal bool volatile IsActive;
internal int Connection;
internal uint32_t volatile FocusedWindowId;
internal uint32_t volatile FocusedWindowPid;
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
send_de_event(ProcessSerialNumber *WindowPsn, uint32_t WindowId)
{
    uint8_t bytes[0xf8] = {
        [0x04] = 0xf8,
        [0x08] = 0x0d,
        [0x8a] = 0x02
    };

    memcpy(bytes + 0x3c, &WindowId, sizeof(uint32_t));
    SLPSPostEventRecordTo(WindowPsn, bytes);
}

internal void
send_re_event(ProcessSerialNumber *WindowPsn, uint32_t WindowId)
{
    uint8_t bytes[0xf8] = {
        [0x04] = 0xf8,
        [0x08] = 0x0d,
        [0x8a] = 0x01
    };

    memcpy(bytes + 0x3c, &WindowId, sizeof(uint32_t));
    SLPSPostEventRecordTo(WindowPsn, bytes);
}

internal void
send_pre_event(ProcessSerialNumber *WindowPsn, uint32_t WindowId)
{
    uint8_t bytes[0xf8] = {
        [0x04] = 0xf8,
        [0x08] = 0x0d,
        [0x8a] = 0x09
    };

    memcpy(bytes + 0x3c, &WindowId, sizeof(uint32_t));
    SLPSPostEventRecordTo(WindowPsn, bytes);
}

internal void
send_post_event(ProcessSerialNumber *WindowPsn, uint32_t WindowId)
{
    uint8_t bytes1[0xf8] = {
        [0x04] = 0xF8,
        [0x08] = 0x01,
        [0x3a] = 0x10
    };

    uint8_t bytes2[0xf8] = {
        [0x04] = 0xF8,
        [0x08] = 0x02,
        [0x3a] = 0x10
    };

    memcpy(bytes1 + 0x3c, &WindowId, sizeof(uint32_t));
    memcpy(bytes2 + 0x3c, &WindowId, sizeof(uint32_t));
    SLPSPostEventRecordTo(WindowPsn, bytes1);
    SLPSPostEventRecordTo(WindowPsn, bytes2);
}

internal inline bool
IsValidWindow(AXUIElementRef *Element, int WindowId)
{
    CFStringRef Role = NULL;
    CFStringRef Subrole = NULL;
    int ElementId = 0;
    bool Result = false;

    if (!AXLibGetWindowRole(*Element, &Role)) {
        goto err;
    }

    if (!CFEqual(Role, kAXWindowRole)) {
        AXUIElementRef WindowRef = (AXUIElementRef) AXLibGetWindowProperty(*Element, kAXWindowAttribute);
        if (WindowRef) {
            CFRelease(*Element);
            *Element = WindowRef;
        } else {
            goto free_role;
        }
    }
    CFRelease(Role);

    ElementId = AXLibGetWindowID(*Element);
    if (ElementId != WindowId) {
        goto err;
    }

    if (!AXLibGetWindowRole(*Element, &Role)) {
        goto err;
    }

    if (!AXLibGetWindowSubrole(*Element, &Subrole)) {
        goto free_role;
    }

    if (!CFEqual(Role, kAXWindowRole) ||
        !CFEqual(Subrole, kAXStandardWindowSubrole)) {
        goto free_subrole;
    }

    Result = true;

free_subrole:
    CFRelease(Subrole);

free_role:
    CFRelease(Role);

err:
    return Result;
}

internal inline void
FocusFollowsMouse(CGEventRef Event)
{
    int WindowId = 0;
    int WindowLevel = 0;
    pid_t WindowPid = 0;
    int WindowConnection = 0;
    CGPoint WindowPosition;
    AXUIElementRef Element;
    ProcessSerialNumber WindowPsn;

    CGPoint CursorPosition = CGEventGetLocation(Event);
    CGSFindWindowByGeometry(Connection, 0, 1, 0, &CursorPosition, &WindowPosition, &WindowId, &WindowConnection);

    if (WindowId == 0)                  return;
    if (Connection == WindowConnection) return;
    if (WindowId == FocusedWindowId)    return;

    CGSGetWindowLevel(Connection, (uint32_t)WindowId, (uint32_t*)&WindowLevel);
    if (!IsWindowLevelAllowed(WindowLevel)) return;
    CGSConnectionGetPID(WindowConnection, &WindowPid);
    GetProcessForPID(WindowPid, &WindowPsn);

    AXUIElementCopyElementAtPosition(SystemWideElement, CursorPosition.x, CursorPosition.y, &Element);
    if (!Element) return;

    if (IsValidWindow(&Element, WindowId)) {
        if (DisableAutoraise) {
            send_pre_event(&WindowPsn, WindowId);
            if (FocusedWindowPid != WindowPid) {
                _SLPSSetFrontProcessWithOptions(&WindowPsn, WindowId, kCPSUserGenerated);
            } else {
                send_de_event(&WindowPsn, FocusedWindowId);
                send_re_event(&WindowPsn, WindowId);
            }
            send_post_event(&WindowPsn, WindowId);
        } else {
            AXLibSetFocusedWindow(Element);
            AXLibSetFocusedApplication(WindowPid);
        }
    }
    CFRelease(Element);
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
        CFRelease(WindowRef);
    }
}

internal inline void
WindowFocusedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    if (AXLibIsWindowStandard(Window)) {
        FocusedWindowId = Window->Id;
        FocusedWindowPid = Window->Owner->PID;
    }
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
CHUNKWM_PLUGIN("Focus Follows Mouse", "0.4.0")
