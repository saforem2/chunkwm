#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/observer.h"
#include "../../common/dispatch/carbon.h"
#include "../../common/dispatch/workspace.h"
#include "overlay.h"

#define internal static

internal macos_application *Application;
internal pid_t LastLaunchedPID;

internal void
UpdateWindow(AXUIElementRef WindowRef)
{
    CGPoint Position = AXLibGetWindowPosition(WindowRef);
    CGSize Size = AXLibGetWindowSize(WindowRef);
    UpdateBorder(Position.x, Position.y, Size.width, Size.height);
}

internal void
UpdateIfFocusedWindow(AXUIElementRef Element)
{
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if(CFEqual(WindowRef, Element))
    {
        UpdateWindow(WindowRef);
    }
    CFRelease(WindowRef);
}

internal
OBSERVER_CALLBACK(Callback)
{
    if(CFEqual(Notification, kAXFocusedWindowChangedNotification))
    {
        UpdateWindow(Element);
    }
    else if(CFEqual(Notification, kAXWindowMovedNotification))
    {
        UpdateIfFocusedWindow(Element);
    }
    else if(CFEqual(Notification, kAXWindowResizedNotification))
    {
        UpdateIfFocusedWindow(Element);
    }
    else if(CFEqual(Notification, kAXWindowMiniaturizedNotification))
    {
        UpdateIfFocusedWindow(Element);
    }
}

internal void
UpdateBorderHelper(macos_application *Application)
{
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if(WindowRef)
    {
        UpdateWindow(WindowRef);
        CFRelease(WindowRef);
    }
    else
    {
        UpdateBorder(0, 0, 0, 0);
    }

    if(AXLibAddApplicationObserver(Application, Callback))
    {
        printf("    plugin: subscribed to '%s' notifications\n", Application->Name);
    }
}

#define MICROSEC_PER_SEC 1e6
macos_application *FrontApplication()
{
    macos_application *Result = AXLibConstructFocusedApplication();
    if(Result)
    {
        if(LastLaunchedPID == Result->PID)
        {
            usleep(0.5 * MICROSEC_PER_SEC);
            UpdateBorderHelper(Result);
            LastLaunchedPID = 0;
        }
        else
        {
            UpdateBorderHelper(Result);
        }
    }
    else
    {
        UpdateBorder(0, 0, 0, 0);
    }

    return Result;
}

void ApplicationLaunchedHandler(const char *Data)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;
    LastLaunchedPID = Info->PID;
}

void ApplicationActivatedHandler(const char *Data)
{
    if(Application)
    {
        AXLibDestroyApplication(Application);
        Application = NULL;
    }

    Application = FrontApplication();
}

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

/*
 * NOTE(koekeishiya): Function parameters
 * const char *Node
 * const char *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(StringsAreEqual(Node, "chunkwm_export_application_activated"))
    {
        ApplicationActivatedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_launched"))
    {
        ApplicationLaunchedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_space_changed"))
    {
        UpdateBorder(0, 0, 0, 0);
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
    printf("Plugin Init!\n");

    CreateBorder(0, 0, 0, 0);
    Application = FrontApplication();
    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    printf("Plugin DeInit!\n");
}

// NOTE(koekeishiya): Enable to manually trigger ABI mismatch
#if 0
#undef PLUGIN_API_VERSION
#define PLUGIN_API_VERSION 0
#endif

// NOTE(koekeishiya): Initialize plugin function pointers.
CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)

// NOTE(koekeishiya): Subscribe to ChunkWM events!
chunkwm_plugin_export Subscriptions[] =
{
    chunkwm_export_application_activated,
    chunkwm_export_application_launched,
    chunkwm_export_space_changed,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Border", "0.0.1")
