### HEAD -  not yet released

#### other changes

 - sanitize plugin filename to prevent directory traversal when a plugin directory has been specified (#604)

 - some features of chunkwm-sa should once again work properly for macOS High Sierra (#611)

----------

### version 0.4.9

 - update spaces support for macOS Mojave 10.14.4 (fast-switch, create and destroy, move to monitor) (#571, #583)

 - use unix domain socket (file) and pid-file instead of INET port when listening to commands (#434)

----------

### version 0.4.8

#### other changes

 - update spaces support for macOS Mojave (fast-switch, create and destroy) (#554)

----------

### version 0.4.7

### other changes

 - some applications are incorrectly classified as support GUI (windows) but do not respond correctly.
   add a maximum wait-time so that we eventually timeout for these applications (#489)

----------

### version 0.4.6

#### ABI Change

 - Add "magic number" to exported plugin header to identify valid chunkwm plugin files

 - Add new log-level *profile* and simple helper macros (#446)

----------

### version 0.4.5


----------

### version 0.4.4

#### other changes

 - add full spaces support for macOS Mojave (fast-switch, create and destroy) (#425)

----------

### version 0.4.3

#### other changes

 - loosen filter applied to launched applications based on process type (#108)

 - we try to subscribe to notifications for sheet-windows (#229)

 - automatically load chunkwm-sa (if installed) on startup or if the Dock is restarted (#448)

 - update bundled chunkwm-sa (#425)

----------

### version 0.4.2

#### other changes

 - update bundled version of chunkwm-sa (#431)

----------

### version 0.4.1

#### other changes

 - update bundled version of chunkwm-sa (#431)

----------

### version 0.4.0

#### ABI Change

 - Add new event `chunkwm_export_window_sheet_created` (#229)

----------

### version 0.3.9

#### other changes

 - check if 'displays have separate spaces' is enabled, and fail with an error message if not (#424)

----------

### version 0.3.8

#### other changes

 - don't force 'chunkc' invocations to require backgrounded processes when ran from config-file (#376)

----------

### version 0.3.7

#### other changes

 - fixed an issue with *chunkc* reading socket response (#376)

 - any 'chunkc' command that is run from the config file
   that is NOT 'chunkc core::<..>' or 'chunkc set ..'
   MUST be put in the background using &

   e.g: `chunkc tiling::rule --owner Finder --name Copy --state float &`

----------

### version 0.3.6

#### other changes

- fixed an issue that caused applications to be ignored by chunkwm due to slow startup-time (#329)

----------

### version 0.3.5

#### other changes

- fixed an issue that caused chunkwm to crash if the user tried to query a non-existing cvar (#378)

----------

### version 0.3.4

#### other changes

- fixed an issue where a window would incorrectly be deemed invalid due to an obscure issue with registering notifications.

----------

### version 0.3.3

#### cvar changes

- added new option to set logging-output in the config-file:
  `chunkc core::log_file <stdout | stderr | /path/to/file>`

----------

### version 0.3.2

#### other changes

- fixed an issue with identifying monitors by arrangement.

----------

### version 0.3.1

#### launch argument

- removed launch argument to set logging-level. logging-level is set in the config-file!

#### other changes

- fixed a race-condition that could cause plugin-commands to be attempted to be ran before
  the plugin was successfully loaded.

- fixed an issue that could cause argument parsing in plugin-commands to fail due to not properly
  resetting the state of *getopt*.

----------

### version 0.3.0

#### launch argument

- added new launch argument to set logging-level:
  ```
  chunkwm --log-level <option>
  <option>: none | debug | warn | error
  ```

#### cvar changes

- added new option to select logging-level during runtime:
  `chunkc core::log_level <none | debug | warn | error>`

#### other changes

- plugin commands are now ran through the main event-queue, for safety-reasons.

----------

### version 0.2.36

changes from previous versions are unavailable..
