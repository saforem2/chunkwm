### HEAD - not yet released

---------------

### version 0.4.0

#### other changes

 - new config setting to disable raise, turning it into real autofocus instead (#332)

 - ffm will now only consider windows that report a proper role of AXWindow and subrole of AXStandardWindow (#332)

---------------

### version 0.3.5

#### other changes

 - undo fix made in previous version because it causes other more annoying issues (#455, #432)

---------------

### version 0.3.4

#### other changes

 - do not incorrectly trigger standby-on-float on some tooltip windows etc (#432)

---------------

### version 0.3.3

#### cvar changes

 - new cvar *ffm_standby_on_float* (#437)

   causes ffm to temporarily be disabled when a floating window is focused.

---------------

### version 0.3.2

#### cvar changes

 - uses *mouse_motion_interval* to set the time between two motion events, in milliseconds.

---------------

### version 0.3.1

#### cvar changes

 - *mouse_modifier* has been renamed to *ffm_bypass_modifier*

---------------

### version 0.3.0

#### other changes

 - improved performance of focus-follows-mouse by reducing the number of calls to the *accessibility API*.

---------------

### version 0.2.3

changes from previous versions are unavailable..
