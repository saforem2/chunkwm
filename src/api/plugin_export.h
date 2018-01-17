#ifndef CHUNKWM_PLUGIN_EXPORT_H
#define CHUNKWM_PLUGIN_EXPORT_H

struct chunkwm_payload
{
    int SockFD;
    char *Command;
    const char *Message;
};

#ifndef CHUNKWM_CORE
enum c_log_level
{
    C_LOG_LEVEL_DEBUG =  0,
    C_LOG_LEVEL_WARN  =  1,
    C_LOG_LEVEL_ERROR =  2,
};
#endif

static const char *chunkwm_plugin_export_str[] =
{
    "chunkwm_export_application_launched",
    "chunkwm_export_application_terminated",
    "chunkwm_export_application_activated",
    "chunkwm_export_application_deactivated",
    "chunkwm_export_application_hidden",
    "chunkwm_export_application_unhidden",

    "chunkwm_export_space_changed",
    "chunkwm_export_display_changed",

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
    "chunkwm_export_window_title_changed",

    "chunkwm_export_count"
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
    chunkwm_export_display_changed,

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
    chunkwm_export_window_title_changed,

    chunkwm_export_count
};

#endif
