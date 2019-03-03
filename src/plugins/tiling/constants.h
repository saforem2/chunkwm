#ifndef PLUGIN_CONSTANTS_H
#define PLUGIN_CONSTANTS_H

#define BUFFER_SIZE                 256

#define CVAR_BAR_ENABLED            "custom_bar_enabled"
#define CVAR_BAR_ALL_MONITORS       "custom_bar_all_monitors"
#define CVAR_BAR_OFFSET_TOP         "custom_bar_offset_top"
#define CVAR_BAR_OFFSET_BOTTOM      "custom_bar_offset_bottom"
#define CVAR_BAR_OFFSET_LEFT        "custom_bar_offset_left"
#define CVAR_BAR_OFFSET_RIGHT       "custom_bar_offset_right"

#define _CVAR_SPACE_MODE            "desktop_mode"
#define _CVAR_SPACE_OFFSET_TOP      "desktop_offset_top"
#define _CVAR_SPACE_OFFSET_BOTTOM   "desktop_offset_bottom"
#define _CVAR_SPACE_OFFSET_LEFT     "desktop_offset_left"
#define _CVAR_SPACE_OFFSET_RIGHT    "desktop_offset_right"
#define _CVAR_SPACE_OFFSET_GAP      "desktop_offset_gap"
#define _CVAR_SPACE_TREE            "desktop_tree"

#define CVAR_SPACE_MODE             "global_" _CVAR_SPACE_MODE
#define CVAR_SPACE_OFFSET_TOP       "global_" _CVAR_SPACE_OFFSET_TOP
#define CVAR_SPACE_OFFSET_BOTTOM    "global_" _CVAR_SPACE_OFFSET_BOTTOM
#define CVAR_SPACE_OFFSET_LEFT      "global_" _CVAR_SPACE_OFFSET_LEFT
#define CVAR_SPACE_OFFSET_RIGHT     "global_" _CVAR_SPACE_OFFSET_RIGHT
#define CVAR_SPACE_OFFSET_GAP       "global_" _CVAR_SPACE_OFFSET_GAP

#define CVAR_PADDING_STEP_SIZE      "desktop_padding_step_size"
#define CVAR_GAP_STEP_SIZE          "desktop_gap_step_size"

#define CVAR_BSP_SPAWN_LEFT         "bsp_spawn_left"
#define CVAR_BSP_OPTIMAL_RATIO      "bsp_optimal_ratio"
#define CVAR_BSP_SPLIT_RATIO        "bsp_split_ratio"
#define CVAR_BSP_SPLIT_MODE         "bsp_split_mode"

#define CVAR_MONITOR_FOCUS_CYCLE    "monitor_focus_cycle"
#define CVAR_WINDOW_FOCUS_CYCLE     "window_focus_cycle"
#define Window_Focus_Cycle_None     "none"
#define Window_Focus_Cycle_Monitor  "monitor"
#define Window_Focus_Cycle_All      "all"

#define CVAR_MOUSE_FOLLOWS_FOCUS    "mouse_follows_focus"
#define Mouse_Follows_Focus_Off     "off"
#define Mouse_Follows_Focus_Intr    "intrinsic"
#define Mouse_Follows_Focus_All     "all"

#define CVAR_MOUSE_MOVE_BINDING     "mouse_move_window"
#define CVAR_MOUSE_RESIZE_BINDING   "mouse_resize_window"
#define CVAR_MOUSE_MOTION_INTERVAL  "mouse_motion_interval"
#define Mouse_Move_Binding          "fn 1"
#define Mouse_Resize_Binding        "fn 2"

#define CVAR_WINDOW_FLOAT_NEXT      "window_float_next"
#define CVAR_WINDOW_REGION_LOCKED   "window_region_locked"

#define CVAR_PRE_BORDER_COLOR       "preselect_border_color"
#define CVAR_PRE_BORDER_WIDTH       "preselect_border_width"
#define CVAR_PRE_BORDER_RADIUS      "preselect_border_radius"
#define CVAR_PRE_BORDER_OUTLINE     "preselect_border_outline"

/*
 * NOTE(koekeishiya): The following cvars requires extended dock
 * functionality provided by chwm-sa to work.
 */

#define CVAR_WINDOW_FLOAT_TOPMOST   "window_float_topmost"
#define CVAR_WINDOW_FADE_INACTIVE   "window_fade_inactive"
#define CVAR_WINDOW_FADE_ALPHA      "window_fade_alpha"
#define CVAR_WINDOW_FADE_DURATION   "window_fade_duration"
#define CVAR_WINDOW_CGS_MOVE        "window_use_cgs_move"

/*   ---------------------------------------------------------   */

/* NOTE(koekeishiya): These cvars are read-only !!               */

#define CVAR_FOCUSED_WINDOW         "_focused_window"
#define CVAR_LAST_FOCUSED_WINDOW    "_last_focused_window"
#define CVAR_BSP_INSERTION_POINT    "_bsp_insertion_point"

#define CVAR_ACTIVE_DESKTOP         "_active_desktop"
#define CVAR_LAST_ACTIVE_DESKTOP    "_last_active_desktop"

/*   ---------------------------------------------------------   */

#endif
