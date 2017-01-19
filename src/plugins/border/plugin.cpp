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

internal ax_application *Application;
internal pid_t LastLaunchedPID;

internal
OBSERVER_CALLBACK(Callback)
{
    if(CFEqual(Notification, kAXFocusedWindowChangedNotification))
    {
        CGPoint Position = AXLibGetWindowPosition(Element);
        CGSize Size = AXLibGetWindowSize(Element);
        UpdateBorder(Position.x, Position.y, Size.width, Size.height);
    }
    else if(CFEqual(Notification, kAXWindowMovedNotification))
    {
        AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
        if(CFEqual(WindowRef, Element))
        {
            CGPoint Position = AXLibGetWindowPosition(WindowRef);
            CGSize Size = AXLibGetWindowSize(WindowRef);
            UpdateBorder(Position.x, Position.y, Size.width, Size.height);
        }
        CFRelease(WindowRef);
    }
    else if(CFEqual(Notification, kAXWindowResizedNotification))
    {
        AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
        if(CFEqual(WindowRef, Element))
        {
            CGPoint Position = AXLibGetWindowPosition(WindowRef);
            CGSize Size = AXLibGetWindowSize(WindowRef);
            UpdateBorder(Position.x, Position.y, Size.width, Size.height);
        }
        CFRelease(WindowRef);
    }
    /*
    else if(CFEqual(Notification, kAXWindowMiniaturizedNotification))
    {
        printf("kAXWindowMiniaturizedNotification\n");
    }
    else if(CFEqual(Notification, kAXWindowDeminiaturizedNotification))
    {
        printf("kAXWindowDeminiaturizedNotification\n");
    }
    */
}

internal void
UpdateBorderHelper(ax_application *Application)
{
    AXUIElementRef WindowRef = AXLibGetFocusedWindow(Application->Ref);
    if(WindowRef)
    {
        CGPoint Position = AXLibGetWindowPosition(WindowRef);
        CGSize Size = AXLibGetWindowSize(WindowRef);
        UpdateBorder(Position.x, Position.y, Size.width, Size.height);
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

ax_application *FrontApplication()
{
    ax_application *Result = AXLibConstructFocusedApplication();
    if(Result)
    {
        if(LastLaunchedPID == Result->PID)
        {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC), dispatch_get_main_queue(),
            ^{
                UpdateBorderHelper(Result);
            });
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

void ApplicationLaunchedHandler(const char *Data, unsigned int DataSize)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;
    LastLaunchedPID = Info->PID;
}

void ApplicationActivatedHandler(const char *Data, unsigned int DataSize)
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
 * unsigned int DataSize
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(Node)
    {
        if(StringsAreEqual(Node, "chunkwm_export_application_activated"))
        {
            ApplicationActivatedHandler(Data, DataSize);
            return true;
        }
        else if(StringsAreEqual(Node, "chunkwm_export_application_launched"))
        {
            ApplicationLaunchedHandler(Data, DataSize);
            return true;
        }
        else if(StringsAreEqual(Node, "chunkwm_export_space_changed"))
        {
            UpdateBorder(0, 0, 0, 0);
            return true;
        }
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
