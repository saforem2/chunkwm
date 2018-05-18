### HEAD - not yet released

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
