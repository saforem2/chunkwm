#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"

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
        macos_application *Application = (macos_application *) Data;
        printf("    plugin template: launched: '%s'\n", Application->Name);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_terminated"))
    {
        macos_application *Application = (macos_application *) Data;
        printf("    plugin template: terminated: '%s'\n", Application->Name);
        return true;
    }

    return false;
}

/*
 * NOTE(koekeishiya):
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    printf("Plugin Init!\n");
    return true;
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    printf("Plugin DeInit!\n");
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
    chunkwm_export_application_terminated,
    chunkwm_export_application_launched,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Plugin Template", "0.0.1")
