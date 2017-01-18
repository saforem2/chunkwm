#ifndef CHUNKWM_PLUGIN_EXPORT_H
#define CHUNKWM_PLUGIN_EXPORT_H

static const char *chunkwm_plugin_export_str[] =
{
    "chunkwm_export_application_launched",
    "chunkwm_export_application_terminated",
    "chunkwm_export_application_activated",
    "chunkwm_export_application_deactivated",
    "chunkwm_export_application_hidden",
    "chunkwm_export_application_unhidden",

    "chunkwm_export_space_changed",

    "chunkwm_export_end",
};
enum chunkwm_plugin_export
{
    chunkwm_export_application_launched,
    chunkwm_export_application_terminated,
    chunkwm_export_application_activated,
    chunkwm_export_application_deactivated,
    chunkwm_export_application_hidden,
    chunkwm_export_application_unhidden,

    chunkwm_export_space_changed,

    chunkwm_export_end,
};

#endif
