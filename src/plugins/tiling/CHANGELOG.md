### version 0.3.0

#### cvar changes

- mouse_follows_focus has been changed to be of type string, formerly a boolean value.
  the new values are as follows: `none`, `intrinsic` and `all`.

- window_float_center has been removed in favour of a new feature. see provided sample config
  for an example of how to reproduce this using the newly added command.

#### new features

- better support for managing floating windows. now features grid functionality (like Moom).
  accessed through the *--grid-layout* command. see updated README for the tiling plugin for more information.

#### removed features

- removed command *--warp-floating*. replaced by *--grid-layout*.

### version 0.2.26

changes from previous versions are unvailable..
