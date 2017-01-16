#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/observer.h"
#include "../../common/dispatch/carbon.h"

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

static
OBSERVER_CALLBACK(Callback)
{
    ax_application *Application = (ax_application *) Reference;

    if(CFEqual(Notification, kAXWindowCreatedNotification))
    {
        printf("kAXWindowCreatedNotification\n");
    }
    else if(CFEqual(Notification, kAXUIElementDestroyedNotification))
    {
        printf("kAXUIElementDestroyedNotification\n");
    }
    else if(CFEqual(Notification, kAXFocusedWindowChangedNotification))
    {
        printf("kAXFocusedWindowChangedNotification\n");
    }
    else if(CFEqual(Notification, kAXWindowMiniaturizedNotification))
    {
        printf("kAXWindowMiniaturizedNotification\n");
    }
    else if(CFEqual(Notification, kAXWindowDeminiaturizedNotification))
    {
        printf("kAXWindowDeminiaturizedNotification\n");
    }
    else if(CFEqual(Notification, kAXWindowMovedNotification))
    {
        printf("kAXWindowMovedNotification\n");
    }
    else if(CFEqual(Notification, kAXWindowResizedNotification))
    {
        printf("kAXWindowResizedNotification\n");
    }
    else if(CFEqual(Notification, kAXTitleChangedNotification))
    {
        printf("kAXWindowTitleChangedNotification\n");
    }
}

ax_application *Application;
void ApplicationLaunchedHandler(const char *Data, unsigned int DataSize)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;

    printf("inside plugin: launched: '%s'\n", Info->ProcessName);
    if(Application)
    {
        AXLibDestroyApplication(Application);
    }

    Application = AXLibConstructApplication(Info->PSN, Info->PID, Info->ProcessName);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC), dispatch_get_main_queue(),
    ^{
        if(AXLibAddApplicationObserver(Application, Callback))
        {
            printf("subscribed to application notifications\n");
        }
    });
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
        if(StringsAreEqual(Node, "chunkwm_export_application_launched"))
        {
            ApplicationLaunchedHandler(Data, DataSize);
            return true;
        }
    }

    return false;
}

/*
 * NOTE(koekeishiya):
 * param: plugin *Plugin
 * return: void
 */
PLUGIN_FUNC(PluginInit)
{
    printf("Plugin Init!\n");
}

/*
 * NOTE(koekeishiya):
 * param: plugin *Plugin
 * return: void
 */
PLUGIN_FUNC(PluginDeInit)
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
    int SubscriptionCount = 4;
    Plugin->Subscriptions =
        (chunkwm_plugin_export *) malloc((SubscriptionCount + 1) * sizeof(chunkwm_plugin_export));
    Plugin->Subscriptions[SubscriptionCount] = chunkwm_export_end;

    Plugin->Subscriptions[--SubscriptionCount] = chunkwm_export_application_unhidden;
    Plugin->Subscriptions[--SubscriptionCount] = chunkwm_export_application_hidden;
    Plugin->Subscriptions[--SubscriptionCount] = chunkwm_export_application_terminated;
    Plugin->Subscriptions[--SubscriptionCount] = chunkwm_export_application_launched;
}

// NOTE(koekeishiya): Enable to manually trigger ABI mismatch
#if 0
#undef PLUGIN_API_VERSION
#define PLUGIN_API_VERSION 0
#endif

CHUNKWM_PLUGIN("Test Plugin", "0.0.1")
