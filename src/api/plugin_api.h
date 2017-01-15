#ifndef CHUNKWM_PLUGIN_API_H
#define CHUNKWM_PLUGIN_API_H

#include "plugin.h"

#define PLUGIN_FUNC(name) void name(plugin *Plugin)
typedef PLUGIN_FUNC(plugin_init_func);
typedef PLUGIN_FUNC(plugin_deinit_func);

#define PLUGIN_MAIN_FUNC(name) \
    bool name(plugin *Plugin,\
            const char *Node,\
            const char *Data,\
            unsigned int DataSize)
typedef PLUGIN_MAIN_FUNC(plugin_main_func);


static const char *chunkwm_plugin_export_str[] =
{
    "chunkwm_export_application_launched",
    "chunkwm_export_application_terminated",
    "chunkwm_export_application_hidden",
    "chunkwm_export_application_unhidden",

    "chunkwm_export_end",
};
enum chunkwm_plugin_export
{
    chunkwm_export_application_launched,
    chunkwm_export_application_terminated,
    chunkwm_export_application_hidden,
    chunkwm_export_application_unhidden,

    chunkwm_export_end,
};

struct plugin
{
    plugin_init_func *Init;
    plugin_deinit_func *DeInit;
    plugin_main_func *Run;

    chunkwm_plugin_export *Subscriptions;
};

#endif
