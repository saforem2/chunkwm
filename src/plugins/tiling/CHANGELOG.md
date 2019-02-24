### HEAD -  not yet released

----------

### version 0.3.15

#### other changes

 - fix switching to empty desktops not working properly through prev/next monitor command (#464)

 - new command to query windows on a given desktop (#477)

----------

### version 0.3.14

#### other changes

 - fixed a potential null reference when querying index of active window in a monocle desktop (#486)

----------

### version 0.3.13

#### other changes

 - query id of focused monitor should correctly work when a window is in macOS fullscreen mode (#462)

 - change how the active desktop is identified for operations that don't directly involve a window (#484)

 - properly manage windows that are hidden before chunkwm is launched (#473)

 - extend 'window --focus' to support focusing by id (#472)

 - commands to query index of active window, and number of windows, in a monocle desktop (#459)

 - allow *FocusMonitor* to work on empty monitors (#464)

 - add additional properties to the *window rules* system (#460, #474)

 - fix space creation / removal for latest Mojave Beta (#439)

----------

### version 0.3.12

#### cvar changes

 - loosen filter applied to launched applications based on process type (#108)

 - treat sheet-windows as we do regular windows (#229)

 - fix issue with detecting Dock padding in different monitor arrangements (#445)

 - implement support for creating and destroying spaces (#425)

----------

### version 0.3.11

#### cvar changes

 - add ability to specify padding for custom bars (#443)

----------

### version 0.3.10

#### other changes

 - fixes an issue that made it impossible to interact with modal windows when "float topmost" was enabled (#229)

 - remove wrongly assumed method of detecting when windows are moved between monitors using methods external to chunkwm (#385, #403)

 - *--grid-layout* should now properly apply desktop padding (#440)

----------

### version 0.3.9

#### feature

 - add command to focus a desktop WITHOUT going through space animations (#127)

  IMPORTANT: requires injecting code into the dock (https://github.com/koekeishiya/chwm-sa)
  IMPORTANT: this feature will only work on High Sierra

#### other changes

 - check for 'displays have separate spaces' have been moved into chunkwm-core (#424)

----------

### version 0.3.8

#### other changes

 - send event when the virtual-space mode of the focused desktop is changed

----------

### version 0.3.7

#### other changes

 - when displays are moved, re-create all window-regions to fit the new arrangement (#414, #368)

 - do not apply the *--state tile* rule if it has already been applied to this window before (#408)

 - only consider rules using the name or except filter when reapplying rules due to window-title changes (#398)

 - add window-rules /modifier/ for following focus to desktop assigned to a window (#397)

----------

### version 0.3.6

#### other changes

 - better calculation of region used for preselection-borders (#395)

 - change how active desktop is decided for window operations (#385)

----------

### version 0.3.5

#### other changes

 - improve detection of windows that are moved between monitors using methods external to chunkwm (#385)

 - replace an incorrectly assumed assertion with a guard (#377)

 - fixed an issue that could lead to a segfault in certain situations (#367)

 - added command to query id of focused window: `chunkc tiling::query --window id`

----------

### version 0.3.4

#### cvar changes

 - *mouse_modifier* is no longer used by the tiling plugin, and has been replaced with a more flexible system.

 - *mouse_move_window* is a new cvar that is used to set the binding to use to move windows using the mouse.
   previously set by *mouse_modifier*.

   to get the same behaviour as before this change, use: `chunkc set mouse_move_window \"fn 1\"`

 - *mouse_resize_window* is a new cvar that is used to set the binding to use to resize windows using the mouse.
   previously set by *mouse_modifier*.

   to get the same behaviour as before this change, use: `chunkc set mouse_resize_window \"fn 2\"`

 - *mouse_motion_interval* is a new cvar that is used to set the time between two motion events in milliseconds.

See updated README for the tiling plugin for more information.

#### other changes

 - expand information sent with the custom event *tiling_focused_window_floating*

 - *--use-insertion-point* command now applies to the current desktop, instead of the current node.

 - *--toggle* now has a new option `chunkc tiling::window --toggle fade` to enable or disable the effect of
   fading inactive windows, properly restoring alpha values when deactivated.

 - *mouse_resize_window* has been extended to also allow resizing of floating windows.

 - *mouse_move_window* is now rate-limited by the *mouse_motion_interval* cvar
   and should make for an improved experience for people who does not utilize chwm-sa.

 - added command to query the uuid of the focused desktop (*chunkc tiling::query --desktop uuid*)

----------

### version 0.3.3

#### other changes

 - fixed a double-free issue caused by memory-ownership differences on El Capitan and new macOS versions.

----------

### version 0.3.2

#### other changes

 - fixed an issue with identifying monitors by arrangement.

 - don't segfault, but fail to load plugin and print an error if `displays have separate spaces` is disabled.

----------

### version 0.3.1

#### other changes

 - fix memory ownership issue in El Capitan, causing a segfault (double-free).

 - fix an invalid access attempt for some windows when initiating mouse-drag on floating windows.

----------

### version 0.3.0

#### cvar changes

 - mouse_follows_focus has been changed to be of type string, formerly a boolean value.
   the new values are as follows: `off`, `intrinsic` and `all`.

 - window_float_center has been removed in favour of a new feature. see provided sample config
   for an example of how to reproduce this using the newly added command.

#### new features

 - better support for managing floating windows. now features grid functionality (like Moom).
   accessed through the *--grid-layout* command. see updated README for the tiling plugin for more information.

#### removed features

 - removed command *--warp-floating*. replaced by *--grid-layout*.

----------

### version 0.2.36

changes from previous versions are unvailable..
