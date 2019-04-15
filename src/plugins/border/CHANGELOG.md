### HEAD - not yet released

#### other changes

 - fixed an issue that caused the border to not draw after a plugin unload and load operation (#569)

----------

### version 0.3.5

#### other changes

 - new cvar `focused_border_outline` to specify border position (#508)

----------

### version 0.3.4

#### other changes

 - fix border not drawing properly on Mojave (#439)

----------

### version 0.3.3

#### other changes

 - changing border color using runtime command should also update cvar `focused_border_color`

----------

### version 0.3.2

#### cvar changes

 - new cvar `focused_border_skip_monocle` to hide border when a monocle desktop is active (#405).

#### other changes

 - the cvar `focused_border_skip_floating` now also affects floating desktops as well (#276).

----------

### version 0.3.1

#### added runtime command

 - command to change border width during runtime: `chunkc border::width <number>`

----------

### version 0.3.0

#### other changes

 - border should not incorrectly pass through to other spaces for some people, causing "ghost" borders.

 - fixed a double-free issue caused by memory-ownership differences on El Capitan and new macOS versions.

 - border should be properly drawn immediately upon plugin-load.

----------

### version 0.2.10

changes from previous versions are unvailable..
