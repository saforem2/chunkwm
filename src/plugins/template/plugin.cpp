#include <stdlib.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"

#define internal static

internal const char *PluginName = "template";
internal const char *PluginVersion = "0.1.0";
internal chunkwm_api API;

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
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(StringsAreEqual(Node, "chunkwm_export_application_launched"))
    {
        macos_application *Application = (macos_application *) Data;
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_terminated"))
    {
        macos_application *Application = (macos_application *) Data;
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
    chunkwm_export_application_terminated,
    chunkwm_export_application_launched,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN(PluginName, PluginVersion);
