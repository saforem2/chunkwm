#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../api/plugin_api.h"

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
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
            printf("inside plugin: launched: '%.*s'\n", DataSize, Data);
            return true;
        }
    }

    return false;
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
        (chunkwm_plugin_export *) malloc(SubscriptionCount * sizeof(chunkwm_plugin_export) + 1);
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
