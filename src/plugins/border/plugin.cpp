#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/display.h"
#include "../../common/border/border.h"

#define internal static

internal macos_application *Application;
internal border_window *Border;

internal void
UpdateWindow(AXUIElementRef WindowRef)
{
    CGPoint Position = AXLibGetWindowPosition(WindowRef);
    CGSize Size = AXLibGetWindowSize(WindowRef);

    if(AXLibIsWindowFullscreen(WindowRef))
    {
        UpdateBorderWindowRect(Border, 0, 0, 0, 0);
    }
    else
    {
        UpdateBorderWindowRect(Border, Position.x, Position.y, Size.width, Size.height);
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
            UpdateBorderWindowRect(Border, 0, 0, 0, 0);
        }
        CFRelease(WindowRef);
    }
    else
    {
        UpdateBorderWindowRect(Border, 0, 0, 0, 0);
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
        UpdateBorderWindowRect(Border, 0, 0, 0, 0);
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
        UpdateWindow(Window->Ref);
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

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

/*
 * NOTE(koekeishiya):
 * parameter: const char *Node
 * parameter: void *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(StringsAreEqual(Node, "chunkwm_export_application_activated"))
    {
        ApplicationActivatedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_deactivated"))
    {
        ApplicationDeactivatedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_destroyed"))
    {
        WindowDestroyedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_focused"))
    {
        WindowFocusedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_moved"))
    {
        WindowMovedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_resized"))
    {
        WindowResizedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_minimized"))
    {
        WindowMinimizedHandler(Data);
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
    Border = CreateBorderWindow(0, 0, 0, 0, 4, 4, 0xffd5c4a1);
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
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Border", "0.0.2")
