# CNCDan - Haptic Pad

[中文复刻、国内采购与 3D 打印说明](README_CN.md)

> This repository is a fork/reproduction of the original [dmcke5/Hapticpad](https://github.com/dmcke5/Hapticpad) project. Original mechanical, PCB and software designs belong to dmcke5 / CNCDan and remain under the original `CERN-OHL-S-2.0` license.

![Alt text](title.png "Haptic Pad")

A 6 button macropad with a display for button labels and a mouse knob with haptic feedback!

[Project Video Link](https://youtu.be/bNUKRJQjuvQ)

#### Features

- 6 programmable macro buttons
- 128x64 OLED display for button labels and icons
- Support for up to 256 profiles for a total of 1536 macros
- Easy XML configuration, no special drivers required
- Macro button combinations can be configured with up to 3 simultaneous buttons or 3 separate button presses with configurable delays between them
- Micro SD storage for button labels and config files
- Haptic feedback mouse wheel with three different modes: Clicky, Twist and Momentum
- RGB ring with configurable colours and 5 different display modes: Halo, Bands, Breath, Rainbow, Solid and Off
- Easy profile switching with up/down profile buttons or profile list display
- The last profile is stored on the SD card, so the macro pad starts on the profile that was last active

#### Bill of Materials

The list below reflects the parts selected for this fork/build. Links point to the exact part or the closest stable manufacturer/distributor page available.

- 1x [Waveshare 2.42-inch 128x64 OLED display](https://www.waveshare.com/product/ai/2.42inch-oled-module.htm)
- 1x [MT6701 magnetic encoder module](https://www.aliexpress.com/item/1005007469177411.html), configured for ABZ incremental output at 1024 PPR
  - Use the encoder's dedicated diametrically/radially magnetized magnet. The 6x1mm magnets below are only for the wheel coupling.
- 1x [2804 100KV brushless gimbal motor](https://www.aliexpress.com/item/1005006008489660.html) with a hollow shaft
- 6x [Kailh Choc V1 low-profile switches](https://www.aliexpress.com/item/1005005066585322.html)
- 1x [Waveshare RP2040-Plus](https://docs.waveshare.com/RP2040-Plus), 16MB without pre-soldered headers (SKU 23503)
- 1x [SparkFun TMC6300 3-phase motor driver board, ROB-21867](https://www.sparkfun.com/sparkfun-brushless-motor-driver-3-phase-tmc6300.html)
- 1x [Micro SD module](https://www.aliexpress.com/item/1005010587984346.html)
  - A FAT32-formatted Micro SD card is also required; this build already has one.
- 4x [6x1mm magnets](https://www.aliexpress.com/item/1005009894772141.html)
- 2x [Panasonic EVQ-Q2K03W tactile buttons](https://www.digikey.com/en/products/detail/panasonic-electronic-components/EVQ-Q2K03W/762893)
  - The original BOM specifies EVQ-Q2B03W. Check installed height and mechanical fit before final assembly.
- 1x [10nF / 50V / 0603 / 103 X7R capacitor, LCSC C307346](https://www.lcsc.com/search?q=C307346) for controller-board C1

The following parts are optional if you want to include the LEDs:

- 20x [Worldsemi WS2812B-B/W, 5050 / PLCC4, LCSC C114586](https://www.lcsc.com/zh-TW/product-detail/RGB-LEDs_Worldsemi-WS2812B-B-W_C114586.html)
  - Do not use the WS2812B 2020 package; it does not match the PCB footprint.
- 20x [0.1uF (100nF) 0603 capacitors](https://www.aliexpress.com/item/32966526545.html)

#### Hardware

- 9x [M3x5x5 threaded inserts](https://www.aliexpress.com/w/wholesale-m3x5x5-threaded-insert.html) (only needed for the printed housing)
- 3x [M3x6 socket-head cap screws](https://www.aliexpress.com/w/wholesale-m3x6-socket-head-screw.html)
- 2x [M3x10 socket-head cap screws](https://www.aliexpress.com/w/wholesale-m3x10-socket-head-screw.html)
- 10x [M2.5x4 socket-head cap screws](https://www.aliexpress.com/w/wholesale-m2.5x4-socket-head-screw.html)
- 4x [M2x6 countersunk screws](https://www.aliexpress.com/w/wholesale-m2x6-countersunk-screw.html) (the original README incorrectly listed M3x6)
- 2x [M2x6 pan-head/self-tapping screws](https://www.aliexpress.com/w/wholesale-m2x6-self-tapping-screw.html) for the encoder mount (missing from the original BOM; an M2x4/6/8 assortment is useful if fit is uncertain)

### Printing Instructions

#### Printed version

Print all files in the `3D Files/STL's` folder. You will need 6 of the keycap file, 2 of the Menu button file and 2 of the PCB spacer file.

> [!WARNING]
> Uploading `Macro Pad - Printed Version.STL` may trigger a thin-wall warning for the main housing because the LED support features are too thin. Do not ignore the warning blindly: request a manual engineering review or thicken the LED supports before printing. If you print the file as-is, inspect the supports carefully and avoid applying force while fitting the LEDs.

#### Machined version

Print all files in the `3D Files/STL's` folder except `Custom Keycap.STL`, `Macro Pad - Printed Version.STL` and `Menu Button Printed.STL`.

Get all of the `.STEP` files in the `3D Files/STEP` folder machined. Do not include the Macropad Assembly `.STEP` file from the main directory, as it is a complete model of the Macro Pad rather than an individual part. If you supply the `MacroPad Housing.PDF` file with the housing, you can have the mounting holes tapped for you.
