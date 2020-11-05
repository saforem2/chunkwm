#### chunkwm-tiling configuration index

* [config variables](#config-variables)
  * [global tiling mode](#set-global-tiling-mode)
  * [specific desktop tiling mode](#set-tiling-mode-for-a-specific-desktop)
  * [default desktop bsp layout](#set-default-bsp-layout-for-a-specific-desktop)
  * [set custom bar offset](#set-custom-bar-offset)
  * [global offset and window gap](#set-global-desktop-offset-and-window-gap)
  * [specific desktop offset and window gap](#set-desktop-offset-and-window-gap-for-a-specific-desktop)
  * [global offset and window gap incremental step](#set-desktop-offset-and-window-gap-incremental-step)
  * [set spawn position of bsp-windows](#spawned-windows-are-tiled-to-the-left)
  * [set optimal ratio used by bsp_split_mode optimal](#set-optimal-ratio-used-by-bsp_split_mode-optimal)
  * [configure how regions are split](#configure-how-regions-are-split)
  * [configure ratio used when regions are split](#configure-ratio-used-when-regions-are-split)
  * [set wrap behaviour for monitor focus command](#set-wrap-behaviour-for-monitor-focus-command)
  * [set wrap behaviour for window focus command](#set-wrap-behaviour-for-window-focus-command)
  * [set state of mouse follows focus](#set-state-of-mouse-follows-focus)
  * [set binding to use for moving windows with the mouse](#set-binding-to-use-for-moving-windows-with-the-mouse)
  * [set binding to use for resizing windows with the mouse](#set-binding-to-use-for-resizing-windows-with-the-mouse)
  * [set minimum interval between two mouse-motion events](#set-minimum-interval-between-two-mouse-motion-events)
  * [float the next window attempted tiled](#the-next-window-attempted-tiled-will-be-made-floating-instead)
  * [constrain window to region size](#constrain-window-to-bsp-region-size)
  * [signal dock to make windows topmost when floated](#signal-dock-to-make-windows-topmost-when-floated)
  * [signal dock to fade inactive windows](#signal-dock-to-fade-inactive-windows)
  * [set the alpha value for faded windows](#set-the-target-alpha-value-for-faded-windows)
  * [set the time in seconds the fade animation should last](#set-the-time-in-seconds-the-fade-animation-should-last)
  * [signal dock to move window when using mouse-drag on floating windows](#signal-dock-to-move-window-when-using-mouse-drag-on-floating-windows)
* [window commands](#window-commands)
  * [close window](#close-window)
  * [focus window](#focus-window)
  * [swap window](#swap-window)
  * [warp window](#warp-window)
  * [grid layout](#grid-layout)
  * [send window to desktop](#send-window-to-desktop)
  * [send window to monitor](#send-window-to-monitor)
  * [use temporary ratio in commands](#use-temporary-ratio-in-commands)
  * [increase region size](#increase-region-size)
  * [decrease region size](#decrease-region-size)
  * [set insertion point for the focused container](#set-bsp-insertion-point-for-the-focused-container)
  * [toggle various window options](#toggle-various-window-options)
* [desktop commands](#desktop-commands)
  * [desktop layout](#desktop-layout)
  * [focus desktop](#focus-desktop)
  * [create desktop](#create-desktop)
  * [destroy desktop](#destroy-desktop)
  * [move desktop](#move-desktop-to-monitor)
  * [rotate desktop](#rotate-desktop)
  * [mirror desktop](#mirror-desktop)
  * [equalize desktop](#equalize-size-of-all-windows-on-desktop)
  * [adjust desktop padding](#adjust-desktop-padding)
  * [adjust desktop gap](#adjust-desktop-window-gap)
  * [toggle desktop offset and gap](#toggle-desktop-offset-and-window-gap)
  * [serialize desktop to file](#serialize-desktop-bsp-tree-to-file)
  * [deserialize desktop from file](#deserialize-desktop-bsp-tree-from-file)
* [monitor commands](#monitor-commands)
  * [focus monitor](#focus-monitor)
* [window rules](#window-rules)
  * [sample rules](#sample-rules)
* [query commands](#query-commands)
  * [query window related](#query-window-related)
      * [query focused window id](#query-focused-window-id)
      * [query focused window name](#query-focused-window-name)
      * [query focused window owner](#query-focused-window-owner)
      * [query focused window tag](#query-focused-window-tag)
      * [query focused window float status](#query-focused-window-float-status)
      * [query window information](#query-window-information)
  * [query desktop related](#query-desktop-related)
      * [query focused desktop id](#query-focused-desktop-id)
      * [query focused desktop uuid](#query-focused-desktop-uuid)
      * [query focused desktop mode](#query-focused-desktop-mode)
      * [query list of windows on focused desktop](#query-list-of-windows-on-focused-desktop)
      * [query index of active window in monocle mode](#query-index-of-active-window-in-monocle-mode)
      * [query number of windows in monocle mode](#query-number-of-windows-in-monocle-mode)
  * [query monitor related](#query-monitor-related)
      * [query focused monitor](#query-focused-monitor-id)
      * [query monitor count](#query-monitor-count)
  * [query windows for desktop](#query-windows-for-desktop)
  * [query desktops for monitor](#query-desktops-for-monitor)
  * [query monitor for desktop](#query-monitor-for-desktop)

---

#### config variables

##### set global tiling mode

    chunkc set global_desktop_mode <option>
    <option>: bsp | monocle | float

##### set tiling mode for a specific desktop

    chunkc set <index>_desktop_mode monocle
    <index>: mission-control index

##### set default bsp-layout for a specific desktop

    chunkc set <index>_desktop_tree <file>
    <index>: mission-control index
    <file>: /path/to/file (e.g: ~/.chunkwm_layouts/dev_1)

##### set custom bar offset

    chunkc set custom_bar_enabled        <option>
    <option>: 1 | 0
    desc: if enabled, apply offsets

    chunkc set custom_bar_all_monitors   <option>
    <option>: 1 | 0
    desc: if enabled, apply offsets for desktops on ALL monitors

    chunkc set custom_bar_offset_top     20
    chunkc set custom_bar_offset_bottom  0
    chunkc set custom_bar_offset_left    0
    chunkc set custom_bar_offset_right   0

##### set global desktop offset and window gap

    chunkc set global_desktop_offset_top     20
    chunkc set global_desktop_offset_bottom  20
    chunkc set global_desktop_offset_left    20
    chunkc set global_desktop_offset_right   20
    chunkc set global_desktop_offset_gap     15

##### set desktop offset and window gap for a specific desktop

    chunkc set 1_desktop_offset_top          190
    chunkc set 1_desktop_offset_bottom       190
    chunkc set 1_desktop_offset_left         190
    chunkc set 1_desktop_offset_right        190
    chunkc set 1_desktop_offset_gap          15

##### set desktop offset and window gap incremental step

    chunkc set desktop_padding_step_size     10.0
    chunkc set desktop_gap_step_size         5.0

##### spawned windows are tiled to the left

    chunkc set bsp_spawn_left <option>
    <option>: 1 | 0
    desc: new windows are tiled as the left-child

##### set optimal ratio used by bsp_split_mode optimal

    chunkc set bsp_optimal_ratio             1.618
    desc: determines if the new split should be horizontal or vertical

##### configure how regions are split

    chunkc set bsp_split_mode                <option>
    <option>: vertical | horizontal | optimal

##### configure ratio used when regions are split

    chunkc set bsp_split_ratio               <option>
    <option>: 0 < value < 1

##### set wrap behaviour for monitor focus command

    chunkc set monitor_focus_cycle           <option>
    <option>: 1 | 0

##### set wrap behaviour for window focus command

    chunkc set window_focus_cycle            <option>
    <option>: none | monitor | all

##### set state of mouse follows focus

    chunkc set mouse_follows_focus           <option>
    <option>: off | intrinsic | all
    desc:
         intrinsic: only trigger from focus events performed by chunkwm.
         all: trigger from all focus events reported by macOS (system-wide)

##### set binding to use for moving windows with the mouse

    chunkc set mouse_move_window             <option>
    <option>: none | fn | shift | alt | cmd | ctrl | mouse_button_index
    desc: arbitrary combination allowed (use whitespace as delimeter).
          left-mouse = 1, right-mouse = 2, middle-mouse = 3

##### set binding to use for resizing windows with the mouse

    chunkc set mouse_resize_window           <option>
    <option>: none | fn | shift | alt | cmd | ctrl | mouse_button_index
    desc: arbitrary combination allowed (use whitespace as delimeter).
          left-mouse = 1, right-mouse = 2, middle-mouse = 3

##### set minimum interval between two mouse-motion events

    chunkc set mouse_motion_interval         <option>
    <option>: floating-point value
    desc: the minimum interval in milliseconds

##### the next window attempted tiled will be made floating instead

    chunkc set window_float_next             <option>
    <option>: 1 | 0

##### constrain window to bsp region size

    chunkc set window_region_locked          <option>
    <option>: 1 | 0

##### signal dock to make windows topmost when floated

    chunkc set window_float_topmost          <option>
    <option>: 1 | 0
    desc: requires chwm-sa (https://github.com/koekeishiya/chwm-sa)

##### signal dock to fade inactive windows

    chunkc set window_fade_inactive          <option>
    <option>: 1 | 0
    desc: requires chwm-sa (https://github.com/koekeishiya/chwm-sa)

##### set the target alpha value for faded windows

    chunkc set window_fade_alpha             <alpha>
    <alpha>: 0 < value < 1

##### set the time in seconds the fade animation should last

    chunkc set window_fade_duration          0.5

##### signal dock to move window when using mouse-drag on floating windows

    chunkc set window_use_cgs_move           <option>
    <option>: 1 | 0
    desc: requires chwm-sa (https://github.com/koekeishiya/chwm-sa)

---

#### window commands

##### close window

    chunkc tiling::window --close
    short flag: -c
    desc: invokes the window close button

##### focus window

    chunkc tiling::window --focus <option>
    <option>: north | east | south | west | prev | next | biggest | <window_id>
    <window_id>: internal id of a window, retrieved with `query desktop ..`
    short flag: -f

##### swap window

    chunkc tiling::window --swap <option>
    <option>: north | east | south | west | prev | next | biggest
    short flag: -s
    desc: swap two windows in-place

##### warp window

    chunkc tiling::window --warp <option>
    <option>: north | east | south | west | prev | next | biggest
    short flag: -w
    desc: move window in direction and modifies layout

##### grid layout

    chunkc tiling::window --grid-layout <option>
    <option>: rows:cols:left:top:width:height
    short flag: -g
    desc: split region to rows:cols grid, windows on left:top grid, have <width> times grid width and <height> times grid height

##### send window to desktop

    chunkc tiling::window --send-to-desktop <option>
    <option>: prev | next | index
    short flag: -d

##### send window to monitor

    chunkc tiling::window --send-to-monitor <option>
    <option>: prev | next | index
    short flag: -m

##### use temporary ratio in commands

    chunkc tiling::window --use-temporary-ratio <ratio>
    <ratio>: 0 < value < 1
    short flags: -r
    desc: must be used in combination with other commands

##### increase region size

    chunkc tiling::window --use-temporary-ratio <ratio> --adjust-window-edge <option>
    <ratio>: 0 < value < 1
    <option>: north | east | south | west
    short flags: -r, -e

##### decrease region size

    chunkc tiling::window --use-temporary-ratio <ratio> --adjust-window-edge <option>
    <ratio>: -1 < value < 0
    <option>: north | east | south | west
    short flags: -r, -e

##### set bsp-insertion point for the focused container

    chunkc tiling::window --use-insertion-point <option>
    <option>: north | east | south | west | cancel
    short flag: -i

    # specify --use-temporary-ratio to set preselect ratio
    chunkc tiling::window --use-temporary-ratio <ratio> --use-insertion-point <option>

##### toggle various window options

    chunkc tiling::window --toggle <option>
    <option>: native-fullscreen | fullscreen | parent | split | float | sticky | fade
    short flag: -t
    desc: option 'sticky' and 'fade' requires chwm-sa (https://github.com/koekeishiya/chwm-sa)

---

#### desktop commands

##### desktop layout

    chunkc tiling::desktop --layout <option>
    <option>: bsp | monocle | float
    short flag: -l

##### focus desktop

    chunkc tiling::desktop --focus <option>
    <option>: mission-control index | prev | next
    short flag: -f
    desc: requires chwm-sa (https://github.com/koekeishiya/chwm-sa) and High Sierra / Mojave

##### create desktop

    chunkc tiling::desktop --create
    short flag: -c
    desc: requires chwm-sa (https://github.com/koekeishiya/chwm-sa) and High Sierra / Mojave

##### destroy desktop

    chunkc tiling::desktop --annihilate
    short flag: -a
    desc: requires chwm-sa (https://github.com/koekeishiya/chwm-sa) and High Sierra / Mojave

##### move desktop to monitor

    chunkc tiling::desktop --move <option>
    <option>: index (1-based) | prev | next
    short flag: -M
    desc: requires chwm-sa (https://github.com/koekeishiya/chwm-sa) and Mojave

##### rotate desktop

    chunkc tiling::desktop --rotate <option>
    <option>: 90 | 180 | 270
    short flag: -r

##### mirror desktop

    chunkc tiling::desktop --mirror <option>
    <option>: vertical | horizontal
    short flag: -m

##### equalize size of all windows on desktop

    chunkc tiling::desktop --equalize
    short flag: -e

##### adjust desktop padding

    chunkc tiling::desktop --padding <option>
    <option>: inc | dec
    short flag: -p

##### adjust desktop window gap

    chunkc tiling::desktop --gap <option>
    <option>: inc | dec
    short flag: -g

##### toggle desktop offset and window gap

    chunkc tiling::desktop --toggle offset
    short flag: -t

##### serialize desktop bsp-tree to file

    chunkc tiling::desktop --serialize <file>
    <file>: /path/to/file
    short flag: -s

##### deserialize desktop bsp-tree from file

    chunkc tiling::desktop --deserialize <file>
    <file>: /path/to/file
    short flag: -d

---

#### monitor commands

##### focus monitor

    chunkc tiling::monitor -f <option>
    <option>: prev | next | index

---

#### window rules

[POSIX-Extended regular syntax grammar](https://www.gnu.org/software/findutils/manual/html_node/find_html/posix_002dextended-regular-expression-syntax.html)

[Available window roles](https://developer.apple.com/documentation/applicationservices/carbon_accessibility/roles?language=objc)

[Available window subroles](https://developer.apple.com/documentation/applicationservices/carbon_accessibility/subroles?language=objc)

[Available window levels](https://github.com/koekeishiya/chunkwm/issues/461)

Remove the lowercase 'k' in front of the role constant to get the string to specify in a rule. The string IS case sensitive.

See the following sections for how to retrieve information about an open window:

 - [query list of windows on focused desktop](https://github.com/koekeishiya/chunkwm/tree/master/src/plugins/tiling#query-list-of-windows-on-focused-desktop)
 - [query window information](https://github.com/koekeishiya/chunkwm/tree/master/src/plugins/tiling#query-window-information)

| filter    | short flag | type      | description                        |
|-----------|:----------:|:---------:|:----------------------------------:|
| --owner   | -o         | POSIX ERE | application name matches pattern   |
| --name    | -n         | POSIX ERE | window name matches pattern        |
| --role    | -r         | FIXED CMP | window mainrole equals role        |
| --subrole | -R         | FIXED CMP | window subrole equals role         |
| --except  | -e         | POSIX ERE | window name does not match pattern |

| properties    | short flag | value                        | description                             |
|---------------|:----------:|:----------------------------:|:---------------------------------------:|
| --state       | -s         | float                        | automatically float window              |
| --state       | -s         | tile                         | force-tile window regardless of AXRole  |
| --state       | -s         | sticky                       | show on all desktops (implicit float)   |
| --state       | -s         | native-fullscreen            | automatically enter native-fullscreen   |
| --desktop     | -d         | mission-control index        | send window to desktop                  |
| --monitor     | -m         | monitor index                | send window to monitor                  |
| --level       | -l         | integer (see link above)     | set window level based on the given key |
| --alpha       | -a         | floating-point (0 <= a <= 1) | set window alpha                        |
| --grid-layout | -g         | same as grid-layout command  | set window dimension through a grid     |

| modifiers        | short flag | affected property | description                          |
|------------------|:----------:|:-----------------:|:------------------------------------:|
| --follow-desktop | -D         | desktop, monitor  | follow focus to the assigned desktop |

##### sample rules

    chunkc tiling::rule --owner \"System Preferences\" --subrole AXStandardWindow --state tile
    chunkc tiling::rule --owner Finder --name Copy --state float
    chunkc tiling::rule --owner Spotify --desktop 5 --follow-desktop
    chunkc tiling::rule --owner Messages --monitor 2 --follow-desktop
    chunkc tiling::rule --owner mpv --state sticky --alpha 0.65 --grid-layout 5:5:4:0:1:1
    chunkc tiling::rule --owner Terminal --state sticky --level 3 --grid-layout 1:1:0:0:1:1

---

#### query commands

##### query window related

##### query focused window id

    chunkc tiling::query --window id
    short flag: w

##### query focused window name

    chunkc tiling::query --window name
    short flag: w

##### query focused window owner

    chunkc tiling::query --window owner
    short flag: w

##### query focused window tag

    chunkc tiling::query --window tag
    short flag: w
    desc: outputs '<window owner> - <window name>'

##### query focused window float status

    chunkc tiling::query --window float
    short flag: w

##### query window information

    chunkc tiling::query --window <window_id>
    <window_id>: internal id of a window, retrieved with `query desktop ..`
    short flag: w

---

##### query desktop related

##### query focused desktop id

    chunkc tiling::query --desktop id
    short flag: d

##### query focused desktop uuid

    chunkc tiling::query --desktop uuid
    short flag: d

##### query focused desktop mode

    chunkc tiling::query --desktop mode
    short flag: d
    desc: outputs 'bsp | monocle | float'

##### query list of windows on focused desktop

    chunkc tiling::query --desktop windows
    short flag: d

##### query index of active window in monocle mode

    chunkc tiling::query --desktop monocle-index
    short flag: d

##### query number of windows in monocle mode

    chunkc tiling::query --desktop monocle-count
    short flag: d

---

##### query monitor related

##### query focused monitor id

    chunkc tiling::query --monitor id
    short flag: m

##### query monitor count

    chunkc tiling::query --monitor count
    short flag: m

##### query windows for desktop

    chunkc tiling::query --windows-for-desktop <desktop id>
    short flag: W

##### query desktops for monitor

    chunkc tiling::query --desktops-for-monitor <monitor id>
    short flag: D

##### query monitor for desktop

    chunkc tiling::query --monitor-for-desktop <desktop id>
    short flag: M
