#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/display.h"
#include "../../common/border/border.h"
#include "../../common/config/tokenize.h"
#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"

#define internal static

internal macos_application *Application;
internal border_window *Border;
internal bool SkipFloating;
internal bool DrawBorder;
internal chunkwm_api API;

internal void
ClearBorderWindow(border_window *Border)
{
    UpdateBorderWindowRect(Border, 0, 0, 0, 0);
}

internal void
UpdateWindow(AXUIElementRef WindowRef)
{
    if(DrawBorder)
    {
        CGPoint Position = AXLibGetWindowPosition(WindowRef);
        CGSize Size = AXLibGetWindowSize(WindowRef);

        if(AXLibIsWindowFullscreen(WindowRef))
        {
            ClearBorderWindow(Border);
        }
        else
        {
            UpdateBorderWindowRect(Border, Position.x, Position.y, Size.width, Size.height);
        }
    }
}

internal void
UpdateToFocusedWindow()
{
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if(WindowRef)
    {
        uint32_t WindowId = AXLibGetWindowID(WindowRef);
        if(WindowId)
        {
            UpdateWindow(WindowRef);
        }
        else
        {
            ClearBorderWindow(Border);
        }
        CFRelease(WindowRef);
    }
    else
    {
        ClearBorderWindow(Border);
    }
}

internal void
UpdateIfFocusedWindow(AXUIElementRef Element)
{
    if(!Application)
    {
        return;
    }

    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if(WindowRef)
    {
        if(CFEqual(WindowRef, Element))
        {
            UpdateWindow(WindowRef);
        }
        CFRelease(WindowRef);
    }
}

internal void
ApplicationActivatedHandler(void *Data)
{
    Application = (macos_application *) Data;
    UpdateToFocusedWindow();
}

internal void
ApplicationDeactivatedHandler(void *Data)
{
    macos_application *Context = (macos_application *) Data;
    if(Application == Context)
    {
        Application = NULL;
        ClearBorderWindow(Border);
    }
}

internal void
WindowFocusedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    if((AXLibIsWindowStandard(Window)) &&
       ((Window->Owner == Application) ||
       (Application == NULL)))
    {
        macos_space *Space;
        bool Success = AXLibActiveSpace(&Space);
        ASSERT(Success);

        if(AXLibSpaceHasWindow(Space->Id, Window->Id))
        {
            UpdateWindow(Window->Ref);
        }

        AXLibDestroySpace(Space);
    }
}

internal void
WindowDestroyedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    if(Window->Owner == Application)
    {
        UpdateToFocusedWindow();
    }
}

internal void
WindowMovedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    UpdateIfFocusedWindow(Window->Ref);
}

internal void
WindowResizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    UpdateIfFocusedWindow(Window->Ref);
}

internal void
WindowMinimizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    if(Window->Owner == Application)
    {
        UpdateToFocusedWindow();
    }
}

internal void
SpaceChangedHandler()
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Space->Type != kCGSSpaceUser)
    {
        ClearBorderWindow(Border);
    }

    AXLibDestroySpace(Space);
}

inline bool
StringEquals(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

internal void
CommandHandler(void *Data)
{
    chunkwm_payload *Payload = (chunkwm_payload *) Data;
    if(StringEquals(Payload->Command, "color"))
    {
        token Token = GetToken(&Payload->Message);
        if(Token.Length > 0)
        {
            unsigned Color = TokenToUnsigned(Token);
            if(Border)
            {
                UpdateBorderWindowColor(Border, Color);
            }
        }
    }
    else if(StringEquals(Payload->Command, "clear"))
    {
        ClearBorderWindow(Border);
    }
}

internal inline void
TilingFocusedWindowFloatStatus(void *Data)
{
    uint32_t Status = *(uint32_t *) Data;
    if(Status)
    {
        DrawBorder = false;
        ClearBorderWindow(Border);
    }
    else
    {
        DrawBorder = true;
        UpdateToFocusedWindow();
    }
}

/*
 * NOTE(koekeishiya):
 * parameter: const char *Node
 * parameter: void *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(StringEquals(Node, "chunkwm_export_application_activated"))
    {
        ApplicationActivatedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_application_deactivated"))
    {
        ApplicationDeactivatedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_destroyed"))
    {
        WindowDestroyedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_focused"))
    {
        WindowFocusedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_moved"))
    {
        WindowMovedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_resized"))
    {
        WindowResizedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_window_minimized"))
    {
        WindowMinimizedHandler(Data);
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_space_changed"))
    {
        SpaceChangedHandler();
        return true;
    }
    else if(StringEquals(Node, "chunkwm_daemon_command"))
    {
        CommandHandler(Data);
        return true;
    }
    else if((StringEquals(Node, "Tiling_focused_window_float")) &&
            (SkipFloating))
    {
        TilingFocusedWindowFloatStatus(Data);
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
    API = ChunkwmAPI;
    BeginCVars(&API);

    CreateCVar("focused_border_color", 0xffd5c4a1);
    CreateCVar("focused_border_width", 4);
    CreateCVar("focused_border_radius", 4);
    CreateCVar("focused_border_skip_floating", 0);

    unsigned Color = CVarUnsignedValue("focused_border_color");
    int Width = CVarIntegerValue("focused_border_width");
    int Radius = CVarIntegerValue("focused_border_radius");
    SkipFloating = CVarIntegerValue("focused_border_skip_floating");
    DrawBorder = !SkipFloating;
    Border = CreateBorderWindow(0, 0, 0, 0, Width, Radius, Color, true);
    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    DestroyBorderWindow(Border);
}

// NOTE(koekeishiya): Enable to manually trigger ABI mismatch
#if 0
#undef CHUNKWM_PLUGIN_API_VERSION
#define CHUNKWM_PLUGIN_API_VERSION 0
#endif

// NOTE(koekeishiya): Initialize plugin function pointers.
CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)

// NOTE(koekeishiya): Subscribe to ChunkWM events!
chunkwm_plugin_export Subscriptions[] =
{
    chunkwm_export_application_activated,
    chunkwm_export_application_deactivated,

    chunkwm_export_window_focused,
    chunkwm_export_window_destroyed,
    chunkwm_export_window_moved,
    chunkwm_export_window_resized,
    chunkwm_export_window_minimized,

    chunkwm_export_space_changed,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Border", "0.2.4")
