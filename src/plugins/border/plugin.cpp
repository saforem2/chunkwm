#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/observer.h"
#include "../../common/dispatch/workspace.h"
#include "overlay.h"

#define internal static

internal ax_application *Application;

internal
OBSERVER_CALLBACK(Callback)
{
    // ax_application *Application = (ax_application *) Reference;

    if(CFEqual(Notification, kAXFocusedWindowChangedNotification))
    {
        CGPoint Position = AXLibGetWindowPosition(Element);
        CGSize Size = AXLibGetWindowSize(Element);
        UpdateBorder(Position.x, Position.y, Size.width, Size.height);
    }
    else if(CFEqual(Notification, kAXWindowMovedNotification))
    {
        AXUIElementRef WindowRef = (AXUIElementRef) AXLibGetWindowProperty(Application->Ref, kAXFocusedWindowAttribute);
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
        AXUIElementRef WindowRef = (AXUIElementRef) AXLibGetWindowProperty(Application->Ref, kAXFocusedWindowAttribute);
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

void ApplicationActivatedHandler(const char *Data, unsigned int DataSize)
{
    workspace_application_details *Info =
        (workspace_application_details *) Data;

    if(Application)
    {
        AXLibDestroyApplication(Application);
        Application = NULL;
    }

    Application = AXLibConstructApplication(Info->PSN, Info->PID, Info->ProcessName);
    if(Application)
    {
        AXUIElementRef WindowRef = (AXUIElementRef) AXLibGetWindowProperty(Application->Ref, kAXFocusedWindowAttribute);
        if(WindowRef)
        {
            CGPoint Position = AXLibGetWindowPosition(WindowRef);
            CGSize Size = AXLibGetWindowSize(WindowRef);
            UpdateBorder(Position.x, Position.y, Size.width, Size.height);
            CFRelease(WindowRef);
        }

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC), dispatch_get_main_queue(),
        ^{
            if(AXLibAddApplicationObserver(Application, Callback))
            {
                printf("    plugin: subscribed to '%s' notifications\n", Application->Name);
            }
        });
    }
}

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
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
        if(StringsAreEqual(Node, "chunkwm_export_application_activated"))
        {
            ApplicationActivatedHandler(Data, DataSize);
            return true;
        }
    }

    return false;
}


/*
 * NOTE(koekeishiya):
 * param: plugin *Plugin
 * return: bool -> true if startup succeeded
 */
PLUGIN_INIT_FUNC(PluginInit)
{
    CreateBorder(200, 200, 400, 400);
    printf("Plugin Init!\n");
    return true;
}

/*
 * NOTE(koekeishiya):
 * param: plugin *Plugin
 * return: void
 */
PLUGIN_DEINIT_FUNC(PluginDeInit)
{
    printf("Plugin DeInit!\n");
}

void InitPluginVTable(plugin *Plugin)
{
    // NOTE(koekeishiya): Initialize plugin function pointers.
    Plugin->Init = PluginInit;
    Plugin->DeInit = PluginDeInit;
    Plugin->Run = PluginMain;

    // NOTE(koekeishiya): Subscribe to ChunkWM events!
    int SubscriptionCount = 1;
    Plugin->Subscriptions =
        (chunkwm_plugin_export *) malloc((SubscriptionCount + 1) * sizeof(chunkwm_plugin_export));
    Plugin->Subscriptions[SubscriptionCount] = chunkwm_export_end;

    Plugin->Subscriptions[--SubscriptionCount] = chunkwm_export_application_activated;
}

// NOTE(koekeishiya): Enable to manually trigger ABI mismatch
#if 0
#undef PLUGIN_API_VERSION
#define PLUGIN_API_VERSION 0
#endif

CHUNKWM_PLUGIN("Border", "0.0.1")
