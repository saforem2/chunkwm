#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/misc/carbon.h"
#include "../../common/misc/workspace.h"
#include "../../common/ipc/daemon.h"

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

#define ALPHA 0.98f
inline void
SetWindowAlpha(uint32_t Id)
{
    int SockFD;
    if(ConnectToDaemon(&SockFD, 5050))
    {
        char Message[255];
        sprintf(Message, "window_alpha %d %f", Id, ALPHA);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}

inline void
ProcessApplicationWindowList(macos_application *Application)
{
    macos_window **WindowList = AXLibWindowListForApplication(Application);
    if(WindowList)
    {
        macos_window **List = WindowList;
        macos_window *Window;
        while((Window = *List++))
        {
            SetWindowAlpha(Window->Id);
            AXLibDestroyWindow(Window);
        }

        free(WindowList);
    }
}

/*
 * NOTE(koekeishiya):
 * parameter: const char *Node
 * parameter: void *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(StringsAreEqual(Node, "chunkwm_export_application_launched"))
    {
        macos_application *Application = (macos_application *) Data;
        ProcessApplicationWindowList(Application);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_window_created"))
    {
        macos_window *Window = (macos_window *) Data;
        SetWindowAlpha(Window->Id);
    }

    return false;
}

/*
 * NOTE(koekeishiya):
 * parameter: plugin_broadcast *Broadcast:
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    uint32_t ProcessPolicy = Process_Policy_Regular | Process_Policy_LSUIElement;
    std::vector<macos_application *> Applications = AXLibRunningProcesses(ProcessPolicy);

    for(size_t Index = 0; Index < Applications.size(); ++Index)
    {
        macos_application *Application = Applications[Index];
        ProcessApplicationWindowList(Application);
    }

    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
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
    chunkwm_export_window_created,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Plugin Transparency", "0.0.2")
