#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <map>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/observer.h"
#include "../../common/dispatch/carbon.h"
#include "../../common/dispatch/workspace.h"
#include "../../common/dispatch/cgeventtap.h"
#include "../../common/ipc/daemon.h"

#define internal static

typedef std::map<pid_t, macos_application *> macos_application_map;
typedef macos_application_map::iterator macos_application_map_it;

internal event_tap EventTap;
internal macos_application_map Applications;
internal pthread_mutex_t ApplicationsMutex;

macos_application_map *BeginApplications()
{
    pthread_mutex_lock(&ApplicationsMutex);
    return &Applications;
}

void EndApplications()
{
    pthread_mutex_unlock(&ApplicationsMutex);
}

macos_application *GetApplicationFromPID(pid_t PID)
{
    macos_application *Result = NULL;

    BeginApplications();
    macos_application_map_it It = Applications.find(PID);
    if(It != Applications.end())
    {
        Result = It->second;
    }
    EndApplications();

    return Result;
}

internal void
AddApplication(macos_application *Application)
{
    BeginApplications();
    macos_application_map_it It = Applications.find(Application->PID);
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
    macos_application_map_it It = Applications.find(PID);
    if(It != Applications.end())
    {
        Applications.erase(It);
    }
    EndApplications();
}

internal macos_window **
ApplicationWindowList(macos_application *Application)
{
    macos_window **WindowList = NULL;

    CFArrayRef Windows = (CFArrayRef) AXLibGetWindowProperty(Application->Ref, kAXWindowsAttribute);
    if(Windows)
    {
        CFIndex Count = CFArrayGetCount(Windows);
        WindowList = (macos_window **) malloc((Count + 1) * sizeof(macos_window *));
        WindowList[Count] = NULL;

        printf("%s windowlist:\n", Application->Name);
        for(CFIndex Index = 0; Index < Count; ++Index)
        {
            AXUIElementRef Ref = (AXUIElementRef) CFArrayGetValueAtIndex(Windows, Index);

            macos_window *Window = AXLibConstructWindow(Application, Ref);
            printf("    %s\n", Window->Name);
            AXLibDestroyWindow(Window);
        }

        CFRelease(Windows);
    }

    return WindowList;
}

internal
OBSERVER_CALLBACK(Callback)
{
    macos_application *Application = (macos_application *) Reference;

    if(CFEqual(Notification, kAXWindowCreatedNotification))
    {
        printf("%s: kAXWindowCreatedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXUIElementDestroyedNotification))
    {
        /* TODO(koekeishiya): If this is an actual window, it should be associated
         * with a valid CGWindowID. HOWEVER, because the window in question has been
         * destroyed. We are unable to utilize this window reference with the AX API.
         *
         * The 'CFEqual()' function can still be used to compare this AXUIElementRef
         * with any existing window refs that we may have. There are a couple of ways
         * we can use to track if an actual window is closed.
         *
         *   a) Store all window AXUIElementRefs in a local cache that we update upon
         *      creation and removal. Requires unsorted container with custom comparator
         *      that uses 'CFEqual()' to match AXUIElementRefs.
         *
         *   b) Instead of tracking 'kAXUIElementDestroyedNotification' for an application,
         *      we have to register this notification separately for every window created.
         *      By doing this, we can pass our own data containing the information necessary
         *      to properly identify and report which window was destroyed.
         *
         * At the very least, we need to know the windowid of the destroyed window. */

        printf("%s: kAXUIElementDestroyedNotification %d\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXFocusedWindowChangedNotification))
    {
        printf("%s: kAXFocusedWindowChangedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXWindowMiniaturizedNotification))
    {
        printf("%s: kAXWindowMiniaturizedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXWindowDeminiaturizedNotification))
    {
        printf("%s: kAXWindowDeminiaturizedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXWindowMovedNotification))
    {
        printf("%s: kAXWindowMovedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXWindowResizedNotification))
    {
        printf("%s: kAXWindowResizedNotification\n", Application->Name);
    }
    else if(CFEqual(Notification, kAXTitleChangedNotification))
    {
        printf("%s: kAXWindowTitleChangedNotification\n", Application->Name);
    }
}

#define MICROSEC_PER_SEC 1e6
void ApplicationLaunchedHandler(const char *Data)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;

    macos_application *Application = AXLibConstructApplication(Info->PSN, Info->PID, Info->ProcessName);
    if(Application)
    {
        printf("    plugin: launched '%s'\n", Info->ProcessName);
        AddApplication(Application);

        /* NOTE(koekeishiya): We need to wait for some amount of time before we can try to
         * observe the launched application. The time to wait depends on how long the
         * application in question takes to finish. Half a second is good enough for
         * most applications so we 'usleep()' as a temporary fix for now, but we need a way
         * to properly defer the creation of observers for applications that require more time.
         *
         * We cannot simply defer the creation automatically using dispatch_after, because
         * there is simply no way to remove a dispatched event once it has been created.
         * We need a way to tell a dispatched event to NOT execute and be rendered invalid,
         * because some applications only live for a very very short amount of time.
         * The dispatched event will then be triggered after a potential 'terminated' event
         * has been received, in which the application reference has been freed.
         *
         * Passing an invalid reference to the AXObserver API does not simply trigger an error,
         * but causes a full on segmentation fault. */

        usleep(0.5 * MICROSEC_PER_SEC);
        if(AXLibAddApplicationObserver(Application, Callback))
        {
            printf("    plugin: subscribed to '%s' notifications\n", Application->Name);
        }

        macos_window **WindowList = ApplicationWindowList(Application);
        free(WindowList);
    }
}

void ApplicationTerminatedHandler(const char *Data)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("    plugin: terminated '%s'\n", Info->ProcessName);
        RemoveApplication(Application->PID);
        AXLibDestroyApplication(Application);
    }
}

void ApplicationHiddenHandler(const char *Data)
{
    workspace_application_details *Info =
        (workspace_application_details *) Data;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("    plugin: hidden '%s'\n", Info->ProcessName);
    }
}

void ApplicationUnhiddenHandler(const char *Data)
{
    workspace_application_details *Info =
        (workspace_application_details *) Data;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("    plugin: unhidden '%s'\n", Info->ProcessName);
    }
}

internal
DAEMON_CALLBACK(DaemonCallback)
{
    printf("    plugin daemon: %s\n", Message);
}

internal
EVENTTAP_CALLBACK(EventCallback)
{
    event_tap *EventTap = (event_tap *) Reference;

    switch(Type)
    {
        case kCGEventTapDisabledByTimeout:
        case kCGEventTapDisabledByUserInput:
        {
            CGEventTapEnable(EventTap->Handle, true);
        } break;
        case kCGEventMouseMoved:
        {
            printf("kCGEventMouseMoved\n");
        } break;
        case kCGEventLeftMouseDown:
        {
            printf("kCGEventLeftMouseDown\n");
        } break;
        case kCGEventLeftMouseUp:
        {
            printf("kCGEventLeftMouseUp\n");
        } break;
        case kCGEventLeftMouseDragged:
        {
            printf("kCGEventLeftMouseDragged\n");
        } break;
        case kCGEventRightMouseDown:
        {
            printf("kCGEventRightMouseDown\n");
        } break;
        case kCGEventRightMouseUp:
        {
            printf("kCGEventRightMouseUp\n");
        } break;
        case kCGEventRightMouseDragged:
        {
            printf("kCGEventRightMouseDragged\n");
        } break;

        default: {} break;
    }

    return Event;
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
    if(StringsAreEqual(Node, "chunkwm_export_application_launched"))
    {
        ApplicationLaunchedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_terminated"))
    {
        ApplicationTerminatedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_hidden"))
    {
        ApplicationHiddenHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_unhidden"))
    {
        ApplicationUnhiddenHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_space_changed"))
    {
        printf("Active Space Changed\n");
        return true;
    }

    return false;
}

internal bool
Init()
{
#if 0
    int Port = 4020;
    EventTap.Mask = ((1 << kCGEventMouseMoved) |
                     (1 << kCGEventLeftMouseDragged) |
                     (1 << kCGEventLeftMouseDown) |
                     (1 << kCGEventLeftMouseUp) |
                     (1 << kCGEventRightMouseDragged) |
                     (1 << kCGEventRightMouseDown) |
                     (1 << kCGEventRightMouseUp));

    bool Result = ((pthread_mutex_init(&ApplicationsMutex, NULL) == 0) &&
                   (StartDaemon(Port, &DaemonCallback)) &&
                   (BeginEventTap(&EventTap, &EventCallback)));
#else
    bool Result = (pthread_mutex_init(&ApplicationsMutex, NULL) == 0);
#endif
    return Result;
}

internal void
Deinit()
{
#if 0
    StopDaemon();
    EndEventTap(&EventTap);
#endif
    pthread_mutex_destroy(&ApplicationsMutex);
}

/*
 * NOTE(koekeishiya):
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    printf("Plugin Init!\n");
    return Init();
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    printf("Plugin DeInit!\n");
    Deinit();
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
    chunkwm_export_application_launched,
    chunkwm_export_application_terminated,
    chunkwm_export_application_hidden,
    chunkwm_export_application_unhidden,
    chunkwm_export_space_changed,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Tiling", "0.0.1")
