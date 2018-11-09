#### chunkwm plugin api v7
--------------------------

### Plugin Structure

A *chunkwm plugin* consists of four essential parts. These are an *init*, *deinit* and
*main* function, as well as a list of notifications that the plugin will subscribe to.

In addition to the above, a plugin must provide both a name and a version.

The first step we have to do when creating a new plugin, is to include the necessary
macros and definitions provided through the plugin api:
```C
#include "../../api/plugin.api.h"
```

This file contains definitions for the init, deinit and main function, and it will
also specify the ABI version that we are building against.

The init function is called after a plugin has been loaded and the ABI version has
been verified. The plugin is passed a struct of type `chunkwm_api` which contains
pointers to various functions that *chunkwm* exposes to the plugins. These functions
allow the plugin to broadcast custom events to other plugins, gives it access to the
*cvar* system, and a logger with different output levels that is controlled through
the config-file.

The init function is defined through the *PLUGIN_BOOL_FUNC* macro and should return
true if initialization succeeded, and false otherwise.

```C
/*
 * parameter: chunkwm_api ChunkwmAPI
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    // API is a global variable of type chunkwm_api
    API = ChunkwmAPI;

    //
    // initialization code
    //

    return true;
}
```

The deinit function is called when a plugin is unloaded and is responsible for teardown
and cleanup. This function is defined through the *PLUGIN_VOID_FUNC* macro.

```C
PLUGIN_VOID_FUNC(PluginDeInit)
{
    //
    // deinitialization code
    //
}
```

The main function is invoked when the plugin is notified by *chunkwm* regarding system
events or events broadcasted by other plugins. This function is defined through the
*PLUGIN_MAIN_FUNC* macro. The return value is currently unused, but should return true
for events that were handled property, and false otherwise.

```C
/*
 * NOTE(koekeishiya):
 * parameter: const char *Node
 * parameter: void *Data
 * return: bool
 */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if (strcmp(Node, "chunkwm_export_application_launched") == 0) {
        macos_application *Application = (macos_application *) Data;
        return true;
    } else if (strcmp(Node, "chunkwm_export_application_terminated") == 0) {
        macos_application *Application = (macos_application *) Data;
        return true;
    }

    return false;
}
```

After the above functions have been implemented, it is time to construct the plugin
entry-point. First, we have to link our function pointers to the functions that have
been implemented. This is done through the *CHUNKWM_PLUGIN_VTABLE* macro.

```C
// NOTE(koekeishiya): Initialize plugin function pointers.
CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)
```

Secondly, we specify which events our plugin should subscribe to, using the
*CHUNKWM_PLUGIN_SUBSCRIBE* macro. The array can remain empty if no events are necessary
for the plugin to do whatever its purpose is.

```C
// NOTE(koekeishiya): Subscribe to ChunkWM events!
chunkwm_plugin_export Subscriptions[] =
{
    chunkwm_export_application_terminated,
    chunkwm_export_application_launched,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)
```

Finally, we are ready to generate the plugin entry-point used by *chunkwm*

```C
// NOTE(koekeishiya): Generate plugin
static const char *PluginName = "template";
static const char *PluginVersion = "0.0.1";
CHUNKWM_PLUGIN(PluginName, PluginVersion);
```

The full plugin-template source can be found [here](https://github.com/koekeishiya/chunkwm/tree/master/src/plugins/template)

--------------------------
### Notifications

#### ChunkWM

```
event: chunkwm_events_subscribed
param: none
fired: when a plugin has been hooked into the event-system.
```

```
event: chunkwm_daemon_command
param: chunkwm_payload *
fired: when a message is routed to a plugin using chunkwm's socket.
```

#### Application

```
event: chunkwm_export_application_launched
param: macos_application *
fired: when an application is launched.
```

```
event: chunkwm_export_application_terminated
param: macos_application *
fired: when an application is terminated.
```

```
event: chunkwm_export_application_activated
param: macos_application *
fired: when an application is activated.
```

```
event: chunkwm_export_application_deactivated
param: macos_application *
fired: when an application is deactivated.
```

```
event: chunkwm_export_application_hidden
param: macos_application *
fired: when an application is hidden.
```

```
event: chunkwm_export_application_unhidden
param: macos_application *
fired: when an application is unhidden.
```

#### Window

```
event: chunkwm_export_window_created
param: macos_window *
fired: when a window is created.
```

```
event: chunkwm_export_window_sheet_created
param: macos_window *
fired: when a window is created.
```

```
event: chunkwm_export_window_destroyed
param: macos_window *
fired: when a window is destroyed.
```

```
event: chunkwm_export_window_focused
param: macos_window *
fired: when a window is given focus.
```

```
event: chunkwm_export_window_moved
param: macos_window *
fired: when the top-left position of a window changes.
```

```
event: chunkwm_export_window_resized
param: macos_window *
fired: when the dimensions of a window changes.
```

```
event: chunkwm_export_window_minimized
param: macos_window *
fired: when a window is minimized to the Dock.
```

```
event: chunkwm_export_window_deminimized
param: macos_window *
fired: when a minimized window is restored from the Dock.
```

```
event: chunkwm_export_window_title_changed
param: macos_window *
fired: when the title text of a window is changed.
```

#### Space

```
event: chunkwm_export_space_changed
param: none
fired: when a space-transition has finished.
```

#### Display

```
event: chunkwm_export_display_added
param: CGDirectDisplayID
fired: when a new display is detected.
```

```
event: chunkwm_export_display_removed
param: CGDirectDisplayID
fired: when a display is removed / disconnected.
```

```
event: chunkwm_export_display_moved
param: CGDirectDisplayID
fired: when a display is moved (arrangement changes).
```

```
event: chunkwm_export_display_resized
param: CGDirectDisplayID
fired: when a display changes resolution.
```

```
event: chunkwm_export_display_changed
param: none
fired: when the display which holds key-input changes.
```
