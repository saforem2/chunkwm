#include <stdio.h>
#include "dispatch/carbon.h"
#include "dispatch/workspace.h"
#include "dispatch/event.h"

#include "plugin.h"

// NOTE(koekeishiya): Application-related callbacks.
EVENT_CALLBACK(Callback_ChunkWM_ApplicationLaunched)
{
    carbon_application_details *Info =
        (carbon_application_details *) Event->Context;
    printf("%d: Launched '%s'\n", Info->PID, Info->ProcessName);

    plugin_list *List = BeginPluginList(chunkwm_export_application_launched);
    for(plugin_list_iter It = List->begin();
        It != List->end();
        ++It)
    {
        plugin *Plugin = It->first;
        Plugin->Run(Plugin,
                    "chunkwm_export_application_launched",
                    (char *)Info,
                    sizeof(*Info));
    }
    EndPluginList(chunkwm_export_application_launched);

    EndCarbonApplicationDetails(Info);
}

EVENT_CALLBACK(Callback_ChunkWM_ApplicationTerminated)
{
    carbon_application_details *Info =
        (carbon_application_details *) Event->Context;
    printf("%d: Terminated '%s'\n", Info->PID, Info->ProcessName);

    EndCarbonApplicationDetails(Info);
}

EVENT_CALLBACK(Callback_ChunkWM_ApplicationActivated)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;
    printf("%d: Activated '%s'\n", Info->PID, Info->ProcessName);

    EndWorkspaceApplicationDetails(Info);
}

EVENT_CALLBACK(Callback_ChunkWM_ApplicationDeactivated)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;
    printf("%d: Deactivated '%s'\n", Info->PID, Info->ProcessName);

    EndWorkspaceApplicationDetails(Info);
}

EVENT_CALLBACK(Callback_ChunkWM_ApplicationVisible)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;
    printf("%d: Show '%s'\n", Info->PID, Info->ProcessName);

    EndWorkspaceApplicationDetails(Info);
}
EVENT_CALLBACK(Callback_ChunkWM_ApplicationHidden)
{
    workspace_application_details *Info =
        (workspace_application_details *) Event->Context;
    printf("%d: Hide '%s'\n", Info->PID, Info->ProcessName);

    EndWorkspaceApplicationDetails(Info);
}

// NOTE(koekeishiya): Display-related callbacks
EVENT_CALLBACK(Callback_ChunkWM_DisplayAdded)
{
}
EVENT_CALLBACK(Callback_ChunkWM_DisplayRemoved)
{
}
EVENT_CALLBACK(Callback_ChunkWM_DisplayMoved)
{
}
EVENT_CALLBACK(Callback_ChunkWM_DisplayResized)
{
}

EVENT_CALLBACK(Callback_ChunkWM_DisplayChanged)
{
}
EVENT_CALLBACK(Callback_ChunkWM_SpaceChanged)
{
}

// NOTE(koekeishiya): Mouse-related callbacks
EVENT_CALLBACK(Callback_ChunkWM_MouseMoved)
{
}

EVENT_CALLBACK(Callback_ChunkWM_LeftMouseDown)
{
}
EVENT_CALLBACK(Callback_ChunkWM_LeftMouseDragged)
{
}
EVENT_CALLBACK(Callback_ChunkWM_LeftMouseUp)
{
}

EVENT_CALLBACK(Callback_ChunkWM_RightMouseDown)
{
}
EVENT_CALLBACK(Callback_ChunkWM_RightMouseDragged)
{
}
EVENT_CALLBACK(Callback_ChunkWM_RightMouseUp)
{
}
