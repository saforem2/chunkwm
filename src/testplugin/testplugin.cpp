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
 * Param: plugin *Plugin
 */
PLUGIN_FUNC(PluginInit)
{
    printf("Plugin Init!\n");
}

/*
 * NOTE(koekeishiya):
 * Param: plugin *Plugin
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
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(Node)
    {
        if(StringsAreEqual(Node, "__test"))
        {
            printf("__test: '%.*s'\n", DataSize, Data);
            return true;
        }
    }

    return false;
}

void InitPluginVTable(plugin *Plugin)
{
    Plugin->Init = PluginInit;
    Plugin->DeInit = PluginDeInit;
    Plugin->Run = PluginMain;
#if 0
    static plugin_vtable VTable[] =
    {
        {
            Test,
            "Test"
        },
        {
            WhatTheFuck,
            "WhatTheFuck"
        },
        {
            NULL,
            "VTABLE_END"
        }
    };
    Plugin->VTable = VTable;
#endif
}

// NOTE(koekeishiya): Enable to manually trigger ABI mismatch
#if 0
#undef PLUGIN_API_VERSION
#define PLUGIN_API_VERSION 0
#endif

CHUNKWM_PLUGIN("Test Plugin", "0.0.1")
