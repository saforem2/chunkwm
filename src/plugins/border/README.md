#### chunkwm-border configuration index

* [config settings](#config-settings)
  * [border color](#set-border-color)
  * [border width](#set-border-width)
  * [border radius](#set-border-radius)
  * [skip border for floating windows and desktops](#skip-border-for-floating-windows-and-desktops)
  * [skip border for monocle desktops](#skip-border-for-monocle-desktops)
* [runtime commands](#runtime-commands)
  * [change border color](#change-border-color)
  * [clear border](#clear-border)

#### config settings

##### set border color

    chunkc set focused_border_color 0xAARRGGBB

##### set border width

    chunkc set focused_border_width 4

##### set border radius

    chunkc set focused_border_radius 4

##### skip border for floating windows and desktops

    chunkc set focused_border_skip_floating <option>
    <option>: 1 | 0

##### skip border for monocle desktops

    chunkc set focused_border_skip_monocle  <option>
    <option>: 1 | 0

#### runtime commands

##### change border color

    chunkc border::color 0xAARRGGBB

##### change border width

    chunkc border::width 8

##### clear border

    chunkc border::clear
