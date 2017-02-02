#include "callback.h"
#include "plugin.h"
#include "wqueue.h"

#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/event.h"

#include <stdio.h>
#include <pthread.h>

#define ProcessPluginList(plugin_export)                   \
    plugin_list *List = BeginPluginList(plugin_export);    \
    for(plugin_list_iter It = List->begin();               \
        It != List->end();                                 \
        ++It)                                              \
    {                                                      \
        plugin *Plugin = It->first;                        \
        Plugin->Run(#plugin_export,                        \
                    (char *) Event->Context);              \
    }                                                      \
    EndPluginList(plugin_export)

#define ProcessPluginListThreaded(plugin_export)           \
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
        Work->Data = (char *) Event->Context;              \
        AddWorkQueueEntry(&Queue,                          \
                          &PluginWorkCallback,             \
                          Work);                           \
    }                                                      \
    EndPluginList(plugin_export);                          \
    CompleteWorkQueue(&Queue)                              \

#define internal static

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

// NOTE(koekeishiya): Application-related callbacks.
CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationLaunched)
{
    carbon_application_details *Info =
        (carbon_application_details *) Event->Context;
    printf("%d: Launched '%s'\n", Info->PID, Info->ProcessName);

#if 0
    ProcessPluginList(chunkwm_export_application_launched);
#else
    ProcessPluginListThreaded(chunkwm_export_application_launched);
#endif

    EndCarbonApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationTerminated)
{
    carbon_application_details *Info =
        (carbon_application_details *) Event->Context;
    printf("%d: Terminated '%s'\n", Info->PID, Info->ProcessName);

#if 0
    ProcessPluginList(chunkwm_export_application_terminated);
#else
    ProcessPluginListThreaded(chunkwm_export_application_terminated);
#endif

    EndCarbonApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationActivated)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;
    printf("%d: Activated '%s'\n", Info->PID, Info->ProcessName);

#if 0
    ProcessPluginList(chunkwm_export_application_activated);
#else
    ProcessPluginListThreaded(chunkwm_export_application_activated);
#endif

    EndWorkspaceApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationDeactivated)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;
    printf("%d: Deactivated '%s'\n", Info->PID, Info->ProcessName);

#if 0
    ProcessPluginList(chunkwm_export_application_deactivated);
#else
    ProcessPluginListThreaded(chunkwm_export_application_deactivated);
#endif

    EndWorkspaceApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationVisible)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;
    printf("%d: Show '%s'\n", Info->PID, Info->ProcessName);

#if 0
    ProcessPluginList(chunkwm_export_application_unhidden);
#else
    ProcessPluginListThreaded(chunkwm_export_application_unhidden);
#endif

    EndWorkspaceApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationHidden)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;
    printf("%d: Hide '%s'\n", Info->PID, Info->ProcessName);

#if 0
    ProcessPluginList(chunkwm_export_application_hidden);
#else
    ProcessPluginListThreaded(chunkwm_export_application_hidden);
#endif

    EndWorkspaceApplicationDetails(Info);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_SpaceChanged)
{
    /* NOTE(koekeishiya): This event does not take an argument. */
#if 0
    ProcessPluginList(chunkwm_export_space_changed);
#else
    ProcessPluginListThreaded(chunkwm_export_space_changed);
#endif
}

// NOTE(koekeishiya): Display-related callbacks
CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayAdded)
{
    CGDirectDisplayID *DisplayId
        = (CGDirectDisplayID *) Event->Context;
    printf("%d: Display added\n", *DisplayId);

#if 0
    ProcessPluginList(chunkwm_export_display_added);
#else
    ProcessPluginListThreaded(chunkwm_export_display_added);
#endif

    free(DisplayId);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayRemoved)
{
    CGDirectDisplayID *DisplayId
        = (CGDirectDisplayID *) Event->Context;
    printf("%d: Display removed\n", *DisplayId);

#if 0
    ProcessPluginList(chunkwm_export_display_removed);
#else
    ProcessPluginListThreaded(chunkwm_export_display_removed);
#endif

    free(DisplayId);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayMoved)
{
    CGDirectDisplayID *DisplayId
        = (CGDirectDisplayID *) Event->Context;
    printf("%d: Display moved\n", *DisplayId);

#if 0
    ProcessPluginList(chunkwm_export_display_moved);
#else
    ProcessPluginListThreaded(chunkwm_export_display_moved);
#endif

    free(DisplayId);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayResized)
{
    CGDirectDisplayID *DisplayId
        = (CGDirectDisplayID *) Event->Context;
    printf("%d: Display resolution changed\n", *DisplayId);


#if 0
    ProcessPluginList(chunkwm_export_display_resized);
#else
    ProcessPluginListThreaded(chunkwm_export_display_resized);
#endif

    free(DisplayId);
}

CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayChanged)
{
    /* NOTE(koekeishiya): This event does not take an argument. */
}
