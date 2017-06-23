## Description [![Build Status](https://travis-ci.org/koekeishiya/chunkwm.svg?branch=master)](https://travis-ci.org/koekeishiya/chunkwm)

**chunkwm** is a tiling window manager for MacOS that uses a plugin architecture, successor to [**kwm**](https://github.com/koekeishiya/kwm)

development is happening on and primarily for MacOS Sierra (10.12.3).

older versions may or may not be compatible and will not be officially supported.

| NAME            | RELEASE | VERSION |
|-----------------|:-------:|:-------:|
| chunkwm-core    | Alpha   | 0.1.0   |
| chunkwm-tiling  | Alpha   | 0.1.0   |
| chunkwm-border  | Alpha   | 0.1.0   |
| chunkwm-ffm     | Alpha   | 0.1.0   |

[click here to view current project status](https://github.com/koekeishiya/chunkwm/issues/16)

![chunkwm_demo](https://cloud.githubusercontent.com/assets/6175959/25564481/6863954c-2db4-11e7-8332-221cecb52ce5.gif)

## Install

The installation process consists of multiple steps, due to how **chunkwm** is structured.

The required steps are as follows:

* install chunkwm-core
* install desired plugins

There are currently three plugins available, developed alongside the **chunkwm-core**.

* chunkwm-tiling
* chunkwm-border
* chunkwm-ffm

These deliver the basic functionality expected of a window manager.

[**khd**](https://github.com/koekeishiya/khd) is an application for creating hotkeys and
is meant to be used together with the window manager, to provide a complete experience.
Other applications that serve the same purpose can of course be used instead.

#### MacPorts

There are no officially maintained ports in the MacPorts repository, however, I have created
ports that can be added locally. If someone is interested in maintaining ports in the official
repository, they are free to do so.

    git clone https://github.com/koekeishiya/portfiles /opt/koekeishiya/portfiles
    add /opt/koekeishiya/portfiles in /opt/local/etc/macports/sources.conf

    sudo port -v selfupdate

    sudo port install chunkwm-core
    ps: notes regarding launchctl and sample config

    sudo port install chunkwm-tiling
    ps: notes regarding sample config

    sudo port install chunkwm-border

    sudo port install chunkwm-ffm

#### Homebrew

As of right now, there are no brew formulae available. Ideally, they would follow the same pattern
as MacPorts. I'm not well versed in creating brew formulae, so this might take a while.
If anyone is interested in doing so, feel free to contact me.

#### Source

requires xcode-8 command line tools.

    git clone https://github.com/koekeishiya/chunkwm

**chunkwm-core**:

    cd chunkwm
    make install
    cp -n bin/chunkwm /usr/local/bin
    cp examples/chunkwmrc ~/.chunkwmrc
    cp examples/com.koekeishiya.chunkwm.plist ~/Library/LaunchAgents

    cd chunkwm/src/chunkc
    make
    cp -n bin/chunkc /usr/local/bin

    launchctl load -w ~/Library/LaunchAgents/com.koekeishiya.chunkwm.plist

**chunkwm-tiling**:

    cd chunkwm/src/plugins/tiling
    make install
    mkdir -p ~/.chunkwm_plugins
    cp -n ../../../plugins/tiling.so ~/.chunkwm_plugins
    cp -n examples/chwmtilingrc ~/.chunkwmtilingrc

**chunkwm-border**:

    cd chunkwm/src/plugins/border
    make install
    mkdir -p ~/.chunkwm_plugins
    cp -n ../../../plugins/border.so ~/.chunkwm_plugins

**chunkwm-ffm**:

    cd chunkwm/src/plugins/ffm
    make install
    mkdir -p ~/.chunkwm_plugins
    cp -n ../../../plugins/ffm.so ~/.chunkwm_plugins

## Configuration

**chunkwm-core** has a very sparse configuration file, located at `$HOME/.chunkwmrc`.

a symlink can be made for people who wish to keep the actual file in some other location.

the config file is simply a bash script that specifies which plugins to load and how they are loaded.

plugins can be loaded and unloaded without having to restart **chunkwm**.

see [**sample config**](https://github.com/koekeishiya/chunkwm/blob/master/examples/chunkwmrc) for further information.

**chunkwm-tiling** is where the magic happens, and so it has a more complex configuration structure.

the config file is also a bash script, located at `$HOME/.chunkwmtilingrc`.

the config simply executes commands that write messages to *chunkwm-tiling*'s socket.
most of these can therefore be applied during runtime, which leaves us with two types of messages.

* cvar (config variable)
* command (runtime interaction)

visit [**chunkwm-tiling reference**](https://github.com/koekeishiya/chunkwm/tree/master/src/plugins/tiling/README.md),
and the [**sample config**](https://github.com/koekeishiya/chunkwm/blob/master/src/plugins/tiling/examples/chwmtilingrc)
for further information

a sample keybinding config file for [**khd**](https://github.com/koekeishiya/khd) is also available [**here**](https://github.com/koekeishiya/chunkwm/tree/master/src/plugins/tiling/examples/khdrc)
