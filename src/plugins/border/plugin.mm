#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/border/border.h"
#include "../../common/config/tokenize.h"
#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"

#include "../../common/accessibility/display.mm"
#include "../../common/accessibility/window.cpp"
#include "../../common/accessibility/element.cpp"
#include "../../common/config/tokenize.cpp"
#include "../../common/config/cvar.cpp"
#include "../../common/border/border.mm"

#define internal static

internal macos_application *Application;
internal border_window *Border;
internal bool SkipFloating;
internal bool DrawBorder;
internal chunkwm_api API;

internal AXUIElementRef
GetFocusedWindow()
{
    AXUIElementRef ApplicationRef, WindowRef;

    ApplicationRef = AXLibGetFocusedApplication();
    if(ApplicationRef)
    {
        WindowRef = AXLibGetFocusedWindow(ApplicationRef);
        CFRelease(ApplicationRef);
        if(WindowRef)
        {
            return WindowRef;
        }
    }

    return NULL;;
}

internal void
CreateBorder(int X, int Y, int W, int H)
{
    unsigned Color = CVarUnsignedValue("focused_border_color");
    int Width = CVarIntegerValue("focused_border_width");
    int Radius = CVarIntegerValue("focused_border_radius");
    Border = CreateBorderWindow(X, Y, W, H, Width, Radius, Color, false);
}

internal inline void
ClearBorderWindow(border_window *Border)
{
    UpdateBorderWindowRect(Border, 0, 0, 0, 0);
}

internal inline void
UpdateWindow(AXUIElementRef WindowRef)
{
    if(DrawBorder && Border)
    {
        if(AXLibIsWindowFullscreen(WindowRef))
        {
            ClearBorderWindow(Border);
        }
        else
        {
            CGPoint Position = AXLibGetWindowPosition(WindowRef);
            CGSize Size = AXLibGetWindowSize(WindowRef);
            UpdateBorderWindowRect(Border, Position.x, Position.y, Size.width, Size.height);
        }
    }
}

internal void
UpdateToFocusedWindow()
{
    AXUIElementRef WindowRef = GetFocusedWindow();
    if(WindowRef)
    {
        uint32_t WindowId = AXLibGetWindowID(WindowRef);
        if(WindowId)
        {
            UpdateWindow(WindowRef);
        }
        else if(Border)
        {
            ClearBorderWindow(Border);
        }
        CFRelease(WindowRef);
    }
    else if(Border)
    {
        ClearBorderWindow(Border);
    }
}

internal void
UpdateIfFocusedWindow(AXUIElementRef Element)
{
    AXUIElementRef WindowRef = GetFocusedWindow();
    if(WindowRef)
    {
        if(CFEqual(WindowRef, Element))
        {
            UpdateWindow(WindowRef);
        }
        CFRelease(WindowRef);
    }
}

internal inline void
ApplicationActivatedHandler(void *Data)
{
    Application = (macos_application *) Data;
    UpdateToFocusedWindow();
}

internal inline void
ApplicationDeactivatedHandler(void *Data)
{
    macos_application *Context = (macos_application *) Data;
    if(Application == Context)
    {
        Application = NULL;
        if(Border)
        {
            ClearBorderWindow(Border);
        }
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
        CFStringRef DisplayRef = AXLibGetDisplayIdentifierFromWindow(Window->Id);
        if(!DisplayRef) DisplayRef = AXLibGetDisplayIdentifierFromWindowRect(Window->Position, Window->Size);
        ASSERT(DisplayRef);

        macos_space *Space = AXLibActiveSpace(DisplayRef);
        if(AXLibSpaceHasWindow(Space->Id, Window->Id))
        {
            UpdateWindow(Window->Ref);
        }

        AXLibDestroySpace(Space);
        CFRelease(DisplayRef);
    }
}

internal inline void
NewWindowHandler()
{
    if(Border) return;

    AXUIElementRef WindowRef = GetFocusedWindow();
    if(WindowRef)
    {
        uint32_t WindowId = AXLibGetWindowID(WindowRef);
        if(WindowId)
        {
            CGPoint Position = AXLibGetWindowPosition(WindowRef);
            CGSize Size = AXLibGetWindowSize(WindowRef);
            CreateBorder(Position.x, Position.y, Size.width, Size.height);
        }
        CFRelease(WindowRef);
    }
}

internal inline void
WindowDestroyedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    if(Window->Owner == Application)
    {
        UpdateToFocusedWindow();
    }
}

internal inline void
WindowMovedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    UpdateIfFocusedWindow(Window->Ref);
}

internal inline void
WindowResizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    UpdateIfFocusedWindow(Window->Ref);
}

internal inline void
WindowMinimizedHandler(void *Data)
{
    macos_window *Window = (macos_window *) Data;
    if(Window->Owner == Application)
    {
        UpdateToFocusedWindow();
    }
}

internal inline void
SpaceChangedHandler()
{
    macos_space *Space;
    bool Success = AXLibActiveSpace(&Space);
    ASSERT(Success);

    if(Border)
    {
        DestroyBorderWindow(Border);
        Border = NULL;
    }

    if(Space->Type == kCGSSpaceUser)
    {
        AXUIElementRef WindowRef = GetFocusedWindow();
        if(WindowRef)
        {
            uint32_t WindowId = AXLibGetWindowID(WindowRef);
            if(WindowId)
            {
                CGPoint Position = AXLibGetWindowPosition(WindowRef);
                CGSize Size = AXLibGetWindowSize(WindowRef);
                CreateBorder(Position.x, Position.y, Size.width, Size.height);
            }
            CFRelease(WindowRef);
        }
    }

    AXLibDestroySpace(Space);
}

internal inline bool
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
        if(Border)
        {
            ClearBorderWindow(Border);
        }
    }
}

internal inline void
TilingFocusedWindowFloatStatus(void *Data)
{
    uint32_t Status = *(uint32_t *) Data;
    if(Status)
    {
        DrawBorder = false;
        if(Border)
        {
            ClearBorderWindow(Border);
        }
    }
    else
    {
        DrawBorder = true;
        UpdateToFocusedWindow();
    }
}

PLUGIN_MAIN_FUNC(PluginMain)
{
    if((StringEquals(Node, "chunkwm_export_application_launched")) ||
       (StringEquals(Node, "chunkwm_export_window_created")) ||
       (StringEquals(Node, "chunkwm_export_application_unhidden")) ||
       (StringEquals(Node, "chunkwm_export_window_deminimized")))
    {
        NewWindowHandler();
        return true;
    }
    else if(StringEquals(Node, "chunkwm_export_application_activated"))
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
    else if((StringEquals(Node, "chunkwm_export_space_changed")) ||
            (StringEquals(Node, "chunkwm_export_display_changed")))
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

PLUGIN_BOOL_FUNC(PluginInit)
{
    API = ChunkwmAPI;
    BeginCVars(&API);

    CreateCVar("focused_border_color", 0xffd5c4a1);
    CreateCVar("focused_border_width", 4);
    CreateCVar("focused_border_radius", 4);
    CreateCVar("focused_border_skip_floating", 0);

    SkipFloating = CVarIntegerValue("focused_border_skip_floating");
    DrawBorder = !SkipFloating;
    CreateBorder(0, 0, 0, 0);
    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    if(Border)
    {
        DestroyBorderWindow(Border);
    }
}

CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)
chunkwm_plugin_export Subscriptions[] =
{
    chunkwm_export_application_launched,
    chunkwm_export_application_unhidden,
    chunkwm_export_application_activated,
    chunkwm_export_application_deactivated,

    chunkwm_export_window_created,
    chunkwm_export_window_focused,
    chunkwm_export_window_destroyed,
    chunkwm_export_window_moved,
    chunkwm_export_window_resized,
    chunkwm_export_window_minimized,
    chunkwm_export_window_deminimized,

    chunkwm_export_space_changed,
    chunkwm_export_display_changed,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)
CHUNKWM_PLUGIN("Border", "0.2.7")
