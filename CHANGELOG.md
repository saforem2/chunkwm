### version 0.3.0

#### launch argument

- added new launch arugment to set logging-level:
  ```
  chunkwm --log-level <option>
  <option>: none | debug | warn | error
  ```

#### cvar changes

- added new option to select logging-level during runtime:
  `chunkc core::logging_level <none | debug | warn | error>`

#### other changes

- plugin commands are now ran through the main event-queue, for safety-reasons.

### version 0.2.26

changes from previous versions are unavailable..
