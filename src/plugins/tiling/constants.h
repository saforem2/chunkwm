#ifndef PLUGIN_CONSTANTS_H
#define PLUGIN_CONSTANTS_H

#define BUFFER_SIZE                 256

#define _CVAR_SPACE_MODE            "space_mode"
#define _CVAR_SPACE_OFFSET_TOP      "space_offset_top"
#define _CVAR_SPACE_OFFSET_BOTTOM   "space_offset_bottom"
#define _CVAR_SPACE_OFFSET_LEFT     "space_offset_left"
#define _CVAR_SPACE_OFFSET_RIGHT    "space_offset_right"
#define _CVAR_SPACE_OFFSET_GAP      "space_offset_gap"

#define CVAR_SPACE_MODE             "global_" _CVAR_SPACE_MODE
#define CVAR_SPACE_OFFSET_TOP       "global_" _CVAR_SPACE_OFFSET_TOP
#define CVAR_SPACE_OFFSET_BOTTOM    "global_" _CVAR_SPACE_OFFSET_BOTTOM
#define CVAR_SPACE_OFFSET_LEFT      "global_" _CVAR_SPACE_OFFSET_LEFT
#define CVAR_SPACE_OFFSET_RIGHT     "global_" _CVAR_SPACE_OFFSET_RIGHT
#define CVAR_SPACE_OFFSET_GAP       "global_" _CVAR_SPACE_OFFSET_GAP

#define _CVAR_BSP_INSERTION_POINT   "_bsp_insertion_point"
#define CVAR_BSP_SPAWN_LEFT         "bsp_spawn_left"
#define CVAR_BSP_OPTIMAL_RATIO      "bsp_optimal_ratio"
#define CVAR_BSP_SPLIT_RATIO        "bsp_split_ratio"
#define CVAR_BSP_SPLIT_MODE         "bsp_split_mode"

#define CVAR_MOUSE_FOLLOWS_FOCUS    "mouse_follows_focus"

#define CVAR_WINDOW_FLOAT_NEXT      "window_float_next"

/* NOTE(koekeishiya): The following cvars requires extended dock
 * functionality provided by chwm-sa to work. */

#define CVAR_WINDOW_FLOAT_TOPMOST   "window_float_topmost"

/*   ---------------------------------------------------------   */

#endif
