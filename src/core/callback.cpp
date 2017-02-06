#include "callback.h"
#include "plugin.h"
#include "wqueue.h"
#include "state.h"

#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/event.h"

#include <stdio.h>
#include <pthread.h>

#define internal static

#define ProcessPluginList(plugin_export, Context)          \
    plugin_list *List = BeginPluginList(plugin_export);    \
    for(plugin_list_iter It = List->begin();               \
        It != List->end();                                 \
        ++It)                                              \
    {                                                      \
        plugin *Plugin = It->first;                        \
        Plugin->Run(#plugin_export,                        \
                    (char *) Context);                     \
    }                                                      \
    EndPluginList(plugin_export)

#define ProcessPluginListThreaded(plugin_export, Context)  \
    plugin_list *List = BeginPluginList(plugin_export);    \
    plugin_work WorkArray[List->size()];                   \
    int WorkCount = 0;                                     \
    for(plugin_list_iter It = List->begin();               \
        It != List->end();                                 \
        ++It)                                              \
    {                                                      \
        plugin_work *Work = WorkArray + WorkCount++;       \
        Work->Plugin = It->first;                          \
        Work->Export = (char *) #plugin_export;            \
        Work->Data = (char *) Context;                     \
        AddWorkQueueEntry(&Queue,                          \
                          &PluginWorkCallback,             \
                          Work);                           \
    }                                                      \
    EndPluginList(plugin_export);                          \
    CompleteWorkQueue(&Queue)                              \

struct plugin_work
{
    plugin *Plugin;
    char *Export;
    char *Data;
};

internal work_queue Queue;

internal
WORK_QUEUE_CALLBACK(PluginWorkCallback)
{
    plugin_work *Work = (plugin_work *) Data;
    Work->Plugin->Run(Work->Export,
                      Work->Data);
}

void BeginCallbackThreads(int Count)
{
    if((Queue.Semaphore = sem_open("work_queue_semaphore", O_CREAT, 0644, 0)) == SEM_FAILED)
    {
        fprintf(stderr,
                "chunkwm: could not create semaphore for work queue, "
                "multi-threading disabled!\n");
        return;
    }

    pthread_t Thread[Count];
    for(int Index = 0;
        Index < Count;
        ++Index)
    {
        pthread_create(&Thread[Count], NULL, &WorkQueueThreadProc, &Queue);
    }
}

internal bool
IsProcessInteractive(carbon_application_details *Info)
{
    bool Result = ((!Info->ProcessBackground) &&
                   (Info->ProcessPolicy != PROCESS_POLICY_LSBACKGROUND_ONLY));
    return Result;
}

// NOTE(koekeishiya): Application-related callbacks.
CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationLaunched)
{
    carbon_application_details *Info =
        (carbon_application_details *) Event->Context;

    if(IsProcessInteractive(Info))
    {
        printf("%d:%s launched\n", Info->PID, Info->ProcessName);
        macos_application *Application = ConstructAndAddApplication(Info->PSN, Info->PID, Info->ProcessName);
#if 0
        ProcessPluginList(chunkwm_export_application_launched, Application);
#else
        ProcessPluginListThreaded(chunkwm_export_application_launched, Application);
#endif
    }
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationTerminated)
{
    carbon_application_details *Info =
        (carbon_application_details *) Event->Context;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("%d:%s terminated\n", Info->PID, Info->ProcessName);
#if 0
        ProcessPluginList(chunkwm_export_application_terminated, Application);
#else
        ProcessPluginListThreaded(chunkwm_export_application_terminated, Application);
#endif
        RemoveAndDestroyApplication(Application);
    }

    EndCarbonApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationActivated)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("%d:%s activated\n", Info->PID, Info->ProcessName);
#if 0
        ProcessPluginList(chunkwm_export_application_activated, Application);
#else
        ProcessPluginListThreaded(chunkwm_export_application_activated, Application);
#endif
    }

    EndWorkspaceApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationDeactivated)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("%d:%s deactivated\n", Info->PID, Info->ProcessName);
#if 0
        ProcessPluginList(chunkwm_export_application_deactivated, Application);
#else
        ProcessPluginListThreaded(chunkwm_export_application_deactivated, Application);
#endif
    }

    EndWorkspaceApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationVisible)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("%d:%s visible\n", Info->PID, Info->ProcessName);
#if 0
        ProcessPluginList(chunkwm_export_application_unhidden, Application);
#else
        ProcessPluginListThreaded(chunkwm_export_application_unhidden, Application);
#endif
    }

    EndWorkspaceApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationHidden)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("%d:%s hidden\n", Info->PID, Info->ProcessName);
#if 0
        ProcessPluginList(chunkwm_export_application_hidden, Application);
#else
        ProcessPluginListThreaded(chunkwm_export_application_hidden, Application);
#endif
    }

    EndWorkspaceApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_SpaceChanged)
{
    /* NOTE(koekeishiya): Applications that are not on our focused space fails to have their
     * existing windows added to our windw collection. We must force update our collection on
     * every space change. Windows that are already tracked is NOT added multiple times. */
    UpdateWindowCollection();

    /* NOTE(koekeishiya): This event does not take an argument. */
#if 0
    ProcessPluginList(chunkwm_export_space_changed, NULL);
#else
    ProcessPluginListThreaded(chunkwm_export_space_changed, NULL);
#endif
}

// NOTE(koekeishiya): Display-related callbacks
CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayAdded)
{
    CGDirectDisplayID *DisplayId
        = (CGDirectDisplayID *) Event->Context;
    printf("%d: display added\n", *DisplayId);
#if 0
    ProcessPluginList(chunkwm_export_display_added, DisplayId);
#else
    ProcessPluginListThreaded(chunkwm_export_display_added, DisplayId);
#endif

    free(DisplayId);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayRemoved)
{
    CGDirectDisplayID *DisplayId
        = (CGDirectDisplayID *) Event->Context;
    printf("%d: display removed\n", *DisplayId);
#if 0
    ProcessPluginList(chunkwm_export_display_removed, DisplayId);
#else
    ProcessPluginListThreaded(chunkwm_export_display_removed, DisplayId);
#endif

    free(DisplayId);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayMoved)
{
    CGDirectDisplayID *DisplayId
        = (CGDirectDisplayID *) Event->Context;
    printf("%d: display moved\n", *DisplayId);
#if 0
    ProcessPluginList(chunkwm_export_display_moved, DisplayId);
#else
    ProcessPluginListThreaded(chunkwm_export_display_moved, DisplayId);
#endif

    free(DisplayId);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayResized)
{
    CGDirectDisplayID *DisplayId
        = (CGDirectDisplayID *) Event->Context;
    printf("%d: display resolution changed\n", *DisplayId);
#if 0
    ProcessPluginList(chunkwm_export_display_resized, DisplayId);
#else
    ProcessPluginListThreaded(chunkwm_export_display_resized, DisplayId);
#endif

    free(DisplayId);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayChanged)
{
    /* NOTE(koekeishiya): This event does not take an argument. */
}

// NOTE(koekeishiya): Window-related callbacks
CHUNKWM_CALLBACK(Callback_ChunkWM_WindowCreated)
{
    macos_window *Window = (macos_window *) Event->Context;
    if(AddWindowToCollection(Window))
    {
        printf("%s:%s window created\n", Window->Owner->Name, Window->Name);
#if 0
        ProcessPluginList(chunkwm_export_window_created, Window);
#else
        ProcessPluginListThreaded(chunkwm_export_window_created, Window);
#endif
        /* NOTE(koekeishiya): When a new window is created, we incorrectly
         * receive the kAXFocusedWindowChangedNotification first, We discard
         * that notification and restore it when we have the window to work with. */
        ConstructEvent(ChunkWM_WindowFocused, Window);
    }
    else
    {
        printf("%s:%s window is not destructible, ignore!\n", Window->Owner->Name, Window->Name);
        AXLibRemoveObserverNotification(&Window->Owner->Observer, Window->Ref, kAXUIElementDestroyedNotification);
        AXLibDestroyWindow(Window);
    }
}

CHUNKWM_CALLBACK(Callback_ChunkWM_WindowDestroyed)
{
    macos_window *Window = (macos_window *) Event->Context;
    printf("%s:%s window destroyed\n", Window->Owner->Name, Window->Name);
#if 0
    ProcessPluginList(chunkwm_export_window_destroyed, Window);
#else
    ProcessPluginListThreaded(chunkwm_export_window_destroyed, Window);
#endif
    RemoveWindowFromCollection(Window);
    AXLibDestroyWindow(Window);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_WindowFocused)
{
    macos_window *Window = (macos_window *) Event->Context;
    printf("%s:%s window focused\n", Window->Owner->Name, Window->Name);
#if 0
    ProcessPluginList(chunkwm_export_window_focused, Window);
#else
    ProcessPluginListThreaded(chunkwm_export_window_focused, Window);
#endif
}

CHUNKWM_CALLBACK(Callback_ChunkWM_WindowMoved)
{
    macos_window *Window = (macos_window *) Event->Context;
    printf("%s:%s window moved\n", Window->Owner->Name, Window->Name);
#if 0
    ProcessPluginList(chunkwm_export_window_moved, Window);
#else
    ProcessPluginListThreaded(chunkwm_export_window_moved, Window);
#endif
}

CHUNKWM_CALLBACK(Callback_ChunkWM_WindowResized)
{
    macos_window *Window = (macos_window *) Event->Context;
    printf("%s:%s window resized\n", Window->Owner->Name, Window->Name);
#if 0
    ProcessPluginList(chunkwm_export_window_resized, Window);
#else
    ProcessPluginListThreaded(chunkwm_export_window_resized, Window);
#endif
}

CHUNKWM_CALLBACK(Callback_ChunkWM_WindowMinimized)
{
    macos_window *Window = (macos_window *) Event->Context;
    printf("%s:%s window minimized\n", Window->Owner->Name, Window->Name);
#if 0
    ProcessPluginList(chunkwm_export_window_minimized, Window);
#else
    ProcessPluginListThreaded(chunkwm_export_window_minimized, Window);
#endif
}

CHUNKWM_CALLBACK(Callback_ChunkWM_WindowDeminimized)
{
    macos_window *Window = (macos_window *) Event->Context;
    printf("%s:%s window deminimized\n", Window->Owner->Name, Window->Name);
#if 0
    ProcessPluginList(chunkwm_export_window_deminimized, Window);
#else
    ProcessPluginListThreaded(chunkwm_export_window_deminimized, Window);
#endif
}

/* NOTE(koekeishiya): If a plugin has stored a pointer to our macos_window structs
 * and tries to access the 'name' member outside of 'PLUGIN_MAIN_FUNC', there will
 * be a race condition. Doing so is considered an error.. */
CHUNKWM_CALLBACK(Callback_ChunkWM_WindowTitleChanged)
{
    macos_window *Window = (macos_window *) Event->Context;
    UpdateWindowTitle(Window);

    printf("%s:%s window title changed\n", Window->Owner->Name, Window->Name);
}
