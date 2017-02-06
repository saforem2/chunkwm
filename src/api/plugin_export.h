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

    "chunkwm_export_display_added",
    "chunkwm_export_display_removed",
    "chunkwm_export_display_moved",
    "chunkwm_export_display_resized",

    "chunkwm_export_window_created",
    "chunkwm_export_window_destroyed",
    "chunkwm_export_window_focused",
    "chunkwm_export_window_moved",
    "chunkwm_export_window_resized",
    "chunkwm_export_window_minimized",
    "chunkwm_export_window_deminimized",

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

    chunkwm_export_display_added,
    chunkwm_export_display_removed,
    chunkwm_export_display_moved,
    chunkwm_export_display_resized,

    chunkwm_export_window_created,
    chunkwm_export_window_destroyed,
    chunkwm_export_window_focused,
    chunkwm_export_window_moved,
    chunkwm_export_window_resized,
    chunkwm_export_window_minimized,
    chunkwm_export_window_deminimized,

    chunkwm_export_end,
};

#endif
