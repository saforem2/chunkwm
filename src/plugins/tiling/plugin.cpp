#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <map>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/observer.h"
#include "../../common/dispatch/carbon.h"
#include "../../common/ipc/daemon.h"

#define internal static

typedef std::map<pid_t, ax_application *> ax_application_map;
typedef std::map<pid_t, ax_application *>::iterator ax_application_map_iter;

internal ax_application_map Applications;
internal pthread_mutex_t ApplicationsMutex;

ax_application_map *BeginApplications()
{
    pthread_mutex_lock(&ApplicationsMutex);
    return &Applications;
}

void EndApplications()
{
    pthread_mutex_unlock(&ApplicationsMutex);
}

ax_application *GetApplicationFromPID(pid_t PID)
{
    ax_application *Result = NULL;

    BeginApplications();
    ax_application_map_iter It = Applications.find(PID);
    if(It != Applications.end())
    {
        Result = It->second;
    }
    EndApplications();

    return Result;
}

internal void
AddApplication(ax_application *Application)
{
    BeginApplications();
    ax_application_map_iter It = Applications.find(Application->PID);
    if(It == Applications.end())
    {
        Applications[Application->PID] = Application;
    }
    EndApplications();
}

internal void
RemoveApplication(pid_t PID)
{
    BeginApplications();
    ax_application_map_iter It = Applications.find(PID);
    if(It != Applications.end())
    {
        Applications.erase(It);
    }
    EndApplications();
}

internal
DAEMON_CALLBACK(DaemonCallback)
{
    printf("    plugin daemon: %s\n", Message);
}

internal bool
Init()
{
    int Port = 4020;
    bool Result = (pthread_mutex_init(&ApplicationsMutex, NULL) == 0) &&
                  (StartDaemon(Port, &DaemonCallback));
    return Result;
}

internal void
Deinit()
{
    StopDaemon();
}

internal
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

void ApplicationLaunchedHandler(const char *Data, unsigned int DataSize)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;

    ax_application *Application = AXLibConstructApplication(Info->PSN, Info->PID, Info->ProcessName);
    if(Application)
    {
        printf("    plugin: launched '%s'\n", Info->ProcessName);
        AddApplication(Application);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC), dispatch_get_main_queue(),
        ^{
            if(AXLibAddApplicationObserver(Application, Callback))
            {
                printf("    plugin: subscribed to '%s' notifications\n", Application->Name);
            }
        });
    }
}

void ApplicationTerminatedHandler(const char *Data, unsigned int DataSize)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;

    ax_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("    plugin: terminated '%s'\n", Info->ProcessName);
        RemoveApplication(Application->PID);
        AXLibDestroyApplication(Application);
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
        if(StringsAreEqual(Node, "chunkwm_export_application_launched"))
        {
            ApplicationLaunchedHandler(Data, DataSize);
            return true;
        }
        else if(StringsAreEqual(Node, "chunkwm_export_application_terminated"))
        {
            ApplicationTerminatedHandler(Data, DataSize);
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
    printf("Plugin Init!\n");
    return Init();
}

/*
 * NOTE(koekeishiya):
 * param: plugin *Plugin
 * return: void
 */
PLUGIN_DEINIT_FUNC(PluginDeInit)
{
    printf("Plugin DeInit!\n");
    Deinit();
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

CHUNKWM_PLUGIN("Tiling", "0.0.1")
