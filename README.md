## Description

**chunkwm** is a tiling window manager for MacOS that uses a plugin architecture.

[click here to view current project status](https://github.com/koekeishiya/chunkwm/issues/16)


## Configuration

**chunkwm** itself has a very sparse configuration file, located at `$HOME/.chunkwmrc`.

A symlink can be made for people who wish to keep the actual file in some other location.

The config file is simply a bash script that specifies which plugins to load and how they are loaded.

See [example config](https://github.com/koekeishiya/chunkwm/blob/master/examples/chunkwmrc)

Plugins can be loaded and unloaded without having to restart **chunkwm**:

```
# if plugin_dir is not set in the config file, the absolutepath is necessary
CHUNKC_SOCKET=3920 chunkc load plugin.so
CHUNKC_SOCKET=3920 chunkc unload plugin.so
```
