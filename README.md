## Description [![Build Status](https://travis-ci.org/koekeishiya/chunkwm.svg?branch=master)](https://travis-ci.org/koekeishiya/chunkwm)

**chunkwm** is a tiling window manager for MacOS that uses a plugin architecture, successor to [**kwm**](https://github.com/koekeishiya/kwm)

development is happening on and primarily for MacOS Sierra (10.12.3).

older versions may or may not be compatible and will not be officially supported.

| NAME            | RELEASE | VERSION |
|-----------------|:-------:|:-------:|
| chunkwm-core    | Alpha   | 0.2.2   |
| chunkwm-tiling  | Alpha   | 0.2.0   |
| chunkwm-border  | Alpha   | 0.2.1   |
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

**IMPORTANT**:

The first time **chunkwm-core** is ran, it will request access to the *accessibility API*.

After access has been granted, the application must be restarted

The **chunkwm-tiling** plugin requires ['displays have separate spaces'](https://support.apple.com/library/content/dam/edam/applecare/images/en_US/osx/separate_spaces.png) to be enabled.

#### MacPorts

There are no officially maintained ports in the MacPorts repository, however, I have created
ports that can be added locally. If someone is interested in maintaining ports in the official
repository, they are free to do so.

    git clone https://github.com/koekeishiya/portfiles /opt/koekeishiya/portfiles
    # add /opt/koekeishiya/portfiles to /opt/local/etc/macports/sources.conf

    sudo port -v selfupdate

    # default variant: chunkwm +tiling +border
    sudo port install chunkwm

    # copy sample config and launchd .plist file as specified in the final *port* notes

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

**chunkwm** uses a bash script as its configuration file and is located at `$HOME/.chunkwmrc`.

a different location can be used by using the `--config | -c` argument.

e.g: `chunkwm --config /opt/local/etc/chunkwm/chunkwmrc`

both the *chunkwm-core* and all plugins are configured in this file !!!

settings that should apply to a plugin should be set before the command to load said plugin.

the valid options for **chunkwm-core** are:

    chunkc core::plugin_dir </path/to/plugins>
    chunkc core::hotload <1 | 0>
    chunkc core::load <plugin>
    chunkc core::unload <plugin>

plugins can be loaded and unloaded without having to restart **chunkwm**.

see [**sample config**](https://github.com/koekeishiya/chunkwm/blob/master/examples/chunkwmrc) for further information.

visit [**chunkwm-tiling reference**](https://github.com/koekeishiya/chunkwm/tree/master/src/plugins/tiling/README.md)

a sample keybinding config file for [**khd**](https://github.com/koekeishiya/khd) is also available [**here**](https://github.com/koekeishiya/chunkwm/tree/master/src/plugins/tiling/examples/khdrc)
