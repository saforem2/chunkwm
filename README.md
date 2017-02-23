## description

**chunkwm** is a tiling window manager for MacOS that uses a plugin architecture.

[click here to view current project status](https://github.com/koekeishiya/chunkwm/issues/16)

## configuration

**chunkwm** itself has a very sparse configuration file, located at `$HOME/.chunkwmrc`.

a symlink can be made for people who wish to keep the actual file in some other location.

the config file is simply a bash script that specifies which plugins to load and how they are loaded.

plugins can be loaded and unloaded without having to restart **chunkwm**:

see [sample config](https://github.com/koekeishiya/chunkwm/blob/master/examples/chunkwmrc)

## development

requires Xcode-8 command line tools.

build **chunkwm**:

    # chunkwm's makefile is in the project directory
    cd <project>

    make            # debug build
    make install    # optimized build

the binary is placed in `<project>/bin`.

build **chunkc**:

    # chunkc is a client to interact with **chunkwm** and **plugins**.
    cd <project>/src/chunkc

    make            # optimized build

the binary is placed in `<project>/chunkc/bin`.

build **plugins**:

    # every plugin provides its own makefile
    cd <project>/src/plugins/<plugin>

    make            # debug build
    make install    # optimized build

the plugin binaries are placed in `<project>/plugins`.
