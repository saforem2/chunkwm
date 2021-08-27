#include <stdlib.h>
#include <string.h>

#include "../../api/plugin_api.h"

#include "../../common/misc/carbon.h"
#include "../../common/misc/workspace.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/element.h"
#include "../../common/accessibility/observer.h"
#include "../../common/ipc/daemon.h"

#include "../../common/misc/carbon.cpp"
#include "../../common/misc/workspace.mm"
#include "../../common/accessibility/application.cpp"
#include "../../common/accessibility/display.mm"
#include "../../common/accessibility/window.cpp"
#include "../../common/accessibility/element.cpp"
#include "../../common/accessibility/observer.cpp"
#include "../../common/ipc/daemon.cpp"

#define internal static

internal const char *PluginName = "purify";
internal const char *PluginVersion = "0.1.0";
internal chunkwm_api API;

internal void
ExtendedDockDisableWindowShadow(uint32_t WindowId)
{
    int SockFD;
    if (ConnectToDaemon(&SockFD, 5050)) {
        char Message[64];
        sprintf(Message, "window_shadow_irreversible %d", WindowId);
        WriteToSocket(Message, SockFD);
    }
    CloseSocket(SockFD);
}
inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

/*
 * NOTE(koekeishiya):
 * parameter: const char *Node
 * parameter: void *Data
 * return: bool
 */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if (StringsAreEqual(Node, "chunkwm_export_application_launched")) {
        macos_application *Application = (macos_application *) Data;
        macos_window **WindowList = AXLibWindowListForApplication(Application);
        if (WindowList) {
            macos_window **List = WindowList;
            macos_window *Window;
            while ((Window = *List++)) {
                ExtendedDockDisableWindowShadow(Window->Id);
                AXLibDestroyWindow(Window);
            }

            free(WindowList);
        }
        return true;
    } else if (StringsAreEqual(Node, "chunkwm_export_window_created")) {
        macos_window *Window = (macos_window *) Data;
        ExtendedDockDisableWindowShadow(Window->Id);
        return true;
    }

    return false;
}

/*
 * NOTE(koekeishiya):
 * parameter: chunkwm_api ChunkwmAPI
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    API = ChunkwmAPI;

    int Count = 0;
    int *WindowList = AXLibAllWindows(&Count);
    if (WindowList) {
        for (int Index = 0; Index < Count; ++Index) {
            ExtendedDockDisableWindowShadow(WindowList[Index]);
        }
        free(WindowList);
    }

    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
}

// NOTE(koekeishiya): Enable to manually trigger ABI mismatch
#if 0
#undef CHUNKWM_PLUGIN_API_VERSION
#define CHUNKWM_PLUGIN_API_VERSION 0
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
CHUNKWM_PLUGIN(PluginName, PluginVersion);
