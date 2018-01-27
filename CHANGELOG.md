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
