# Bobot_2_Amethyst
Second robot from Bobot series of robots. A huge amount of things in this robot address problems of previous Bobot.

# Features 

## Mechanical

### Handle

#### Description

On the top there is the handle which is hidden in base.
It rotates out on bearings.
Rotation is locked by mechanism on top of base.

### Back Buttons

#### Description

On the back there are 9 buttons to interact with bobot.
They are with 3d printed tops with symbols representing purpose.

Buttons:
```
 _________________________________
|    Back    |  Up  |     UI      |
|____________|______|_____________|
|    Left    |  OK  |    Right    |
|____________|______|_____________|
|  Settings  | Down |    Debug    |
|____________|______|_____________|
```

### Power Switch

#### Description

At the bottom of left side of bobot(looking at their face) there is power switch, which is connect batteries to power regulation module.

### Screwdriver With Built In Case

#### Description

At the left side of bobot there is case with screwdriver which can unscrew all screws in bobot.
It is based on push latch mechanism, to be held inside of bobot.

### Access panel

#### Description

At the right side there is panel on 2 screws which give access to main microcontroller esp32 dev kit board. When this panel taken apart, esp32 can be fully removed from socket and access to micro sd is revealed.

## Hardware

### Shortcut Components List

| Component | Quantity| Description | Datasheet | Store |
|-------------|-------|-------------|------------|-------------|
|ESP32 unnamed DevKit board|1|Microcontroller|-|-|
|BQ25895 | 1 | power managment ic | [datasheet](https://www.ti.com/lit/ds/symlink/bq25895.pdf) | [store](https://www.aliexpress.com/item/1005008068013158.html) |
| MCP23017 | 1 | 16-Bit I/O Expander with Serial Interface(16 parallel pins to I2C) | [datasheet](https://ww1.microchip.com/downloads/en/devicedoc/20001952c.pdf) | [store](https://www.aliexpress.com/item/1005006974304942.html) |
| MPR121QR2  | 1 | 12 touch buttons driver with I2C interface | [datasheet](https://files.seeedstudio.com/wiki/Grove-I2C_Touch_Sensor/res/Freescale_Semiconductor;MPR121QR2.pdf) | [store](https://www.aliexpress.com/item/1005006944721047.html) |
| TL2285OA  | 9 | Push latch button without fixation | [datasheet](https://www.alldatasheet.com/html-pdf/437236/E-SWITCH/TL2285OA/385/1/TL2285OA.html) | [store](https://www.aliexpress.com/item/1005006775951751.html) |
| MAX98357A | 2 | Chip for audio output: DAC + amplifier in one chip | - | [store](https://www.aliexpress.com/item/1005010444800168.html) |
| BMI160 | 1 | Sensor gyroscope + accelerometer 6DOF | - | [store](https://www.aliexpress.com/item/1005009421405155.html) |

| Component | Quantity| Store |
|-------------|-------|-------------|
|Resistor 150Ω|1|[store](https://www.aliexpress.com/item/1005008789698065.html)|
|Resistor 4.7kΩ|6|[store](https://www.aliexpress.com/item/1005008789698065.html)|
|Resistor 5.1kΩ|1|[store](https://www.aliexpress.com/item/1005008789698065.html)|
|Resistor 10kΩ|18|[store](https://www.aliexpress.com/item/1005008789698065.html)|
|Resistor 33kΩ|1|[store](https://www.aliexpress.com/item/1005008789698065.html)|
|Resistor 75kΩ|1|[store](https://www.aliexpress.com/item/1005008789698065.html)|
|Capacitor 47nF|1|[store](https://www.aliexpress.com/item/1005010516436707.html)|
|Capacitor 100nF|2|[store](https://www.aliexpress.com/item/1005010516436707.html)|
|Capacitor 1uF|10|[store](https://www.aliexpress.com/item/1005010516436707.html)|
|Capacitor 4.7uF|1|[store](https://www.aliexpress.com/item/1005010516436707.html)|
|Capacitor 10uF|4|[store](https://www.aliexpress.com/item/1005010516436707.html)|
|Inductor 2.2uH|1|[store](https://www.aliexpress.com/item/1005001699576419.html)|
|Thermistor B3435 MF52D 10K|1|[store](https://www.aliexpress.com/item/1005006804356266.html)|



### ESP32 Microcontroller

#### Description

Main microcontroller will be unnamed clone of esp32 wroom dev kit with `30 pins`.
It will be placed in sockets on pcb on right side of bobot.

#### Links

[Overview](https://www.espboards.dev/esp32/esp32-30pin-devkit-generic/)

[Datasheet](https://documentation.espressif.com/esp32_datasheet_en.pdf)

[Technical Reference Manual](http://documentation.espressif.com/esp32_technical_reference_manual_en.pdf)

### 2.42 Inch OLED I2C Display

#### Description

There is 2.42 inch OLED display in front side of bobot.
Esp32 communicate with display using `i2c` protocol on
esp32 dedicated i2c pins. This display use SSD1309 driver.

#### Pinout

| Pin / Signal | Source / Connection | GPIO / Net | Description |
|-------------|---------------------|------------|-------------|
| SDA | ESP32 | GPIO21 | I²C data line (main I²C bus) |
| SCL | ESP32 | GPIO22 | I²C clock line (main I²C bus) |


#### Links

[General information](https://turkish.chenghaolcd.com/doc/44205281/2-42-oled-display-module-ic-i2c-spi-serial-with-ssd1309-controller.pdf)

### Li-Po 3000mAh 3.7V Battery 1S2P

#### Description

There are two parallely connecteed li-po battery in bobot, which in total give 6000mAh battery capacity.

#### Links

[Buy](https://prom.ua/ua/p2843472521-litij-polimernyj-akkumulyator.html)

### BQ25895 Battery Management Module

#### Description

Bobot have power management module, which will consist of: 
- `BQ25895` I2C Controlled Single Cell 5-A Fast Charger with MaxChargeTM for High Input
Voltage and Adjustable Voltage 3.1-A Boost Operation
- Thermistor as BQ25895 requires placed onto battery with
  RTHcold = 27 kΩ at 0 °C
  and RTHhot = 4.9 kΩ at 45 °C values
- Led to indicate charge status
- Connection to esp32 I2C bus and INT pin support

And will do:
- Active conversation with Esp32 with transfering measurements of voltages and current(battery monitor feature)

#### Configurations

Hardware:

1. CE pin tied to GND to enable charging
2. Using resistors RT1=5.1 kΩ and RT2=33 kΩ to make BQ25895 read thermistor values correct
3. Led going from SYS to STAT through 2.2kΩ resistor
4. Connecting ILIM pin to GND through Rilim=150 Ω resistor. Calculated using formula from datasheet at section 8.2.12. 150 Ω resistor limit input current to ~2.6A.
5. OTG pin connected to GND to disable boost mode activation

Software:

1. Set register CONV_RATE to 1 to enable active conversation between BQ25895 and ESP32
2. <mark>Set register IINLIM to value representing \<DESIRED_INPUT_CURRENT>
3. <mark>Set TREG to \<DESIRED_MAXIMAL_IC_TEMPERATURE> to regulate at which temperature BQ25895 will lower down charging current

#### Pinout

| Pin / Signal | Connection | Net / GPIO | Description |
|-------------|------------|------------|-------------|
| SDA | ESP32 | GPIO21 | I²C data line for charger configuration/monitoring. |
| SCL | ESP32 | GPIO22 | I²C clock line for charger configuration/monitoring. |
| INT | ESP32 | GPIO (TBD) | Open-drain interrupt (active-low) for charge/fault events; 10 kΩ pull-up to 3.3 V. |
| VBUS | USB Type-C port | VBUS | Charger input supply (3.9–14 V). Decouple close to pin. |
| D+ | USB only | Connect only to USB connector | Used internally for BC1.2/MaxCharge detection. |
| D− | USB only | Connect only to USB connector | Same as D+. |
| STAT | Status LED | — | Open-drain charge status output; LED + resistor to 3.3 V. |
| TS | NTC divider | NTC | Battery temperature sense via NTC near battery; safety-critical. |
| BAT | Battery pack | BAT+ | Battery positive terminal (1S Li-ion/Li-poly). |
| SYS | System rail | SYS | Managed system supply; feeds 3.3 V regulator for ESP32. |
| PGND | Power ground | GND | High-current ground return for charger, battery, SYS, PMID caps. |
| CE | GND | — | Charge enable (active-low); tied to GND for always-enabled charging. |
| ILIM | Resistor → GND | R_ILIM | Hardware input-current limit set to ~3 A (protects USB source). |
| SW | Inductor | SW | Switching node to inductor (keep short, noisy). |
| BTST | Bootstrap cap | BTST | Bootstrap capacitor to SW for high-side gate drive. |
| REGN | Decoupling cap | REGN | Internal LDO output; bias rail (decouple to GND). |
| EPAD (PowerPAD) | PCB copper | PGND | Thermal + electrical ground; must be soldered to PGND with vias. | 
| OTG | Disabled | Tie to GND | Prevents boost (OTG) mode activation. |
| PMID | Not exported | Local ceramic caps to PGND only (≥8.2 µF) | Do not power loads; keep short return. |
| QON | Unused | Leave unconnected | Internal pull-up keeps default behavior. |
| DSEL | Unused | Leave unconnected | Not needed without external USB data switch. |

### Button Board Based On MCP23017 I/O Expander

#### Description

The MCP23017 is a 16-bit I/O expander used to interface with the 9 back panel buttons. It communicates with the ESP32 via I2C at address <mark>I2C_ADDRESS_TBD</mark>. The device provides interrupt-on-change functionality through its INTA and INTB pins, allowing the ESP32 to detect button presses efficiently without constant polling.

Buttons:
```
 _________________________________
|    Back    |  Up  |     UI      |
|____________|______|_____________|
|    Left    |  OK  |    Right    |
|____________|______|_____________|
|  Settings  | Down |    Debug    |
|____________|______|_____________|
```

#### Configurations

Hardware:

1. I2C address set via A0, A1, A2 pins: <mark>ADDRESS_CONFIG_TBD</mark>
2. INTA and INTB pins pulled up to 3.3V through 4.7 kΩ resistors
3. NOT RESET pin tied to 3.3V to keep device enabled
4. Each button input uses a low-pass filter with:
   - 10 kΩ pull-up resistor to 3.3V (default state: logic HIGH)
   - 10 kΩ series resistor between button and IC pin
   - 1 µF capacitor to GND for debouncing
5. Buttons are in high-Z state (disconnected) by default, connect to GND when pressed (active-low)

Software:

1. Configure GPA0-GPA7 and GPB0 as inputs with interrupt-on-change enabled
2. Set up interrupt polarity and configure interrupt mirroring if needed


#### Pinout

| Pin / Signal | Connection | Net / GPIO | Description |
|-------------|------------|------------|-------------|
| SDA | ESP32 | GPIO21 | I²C data line (shared with display and BMS). |
| SCL | ESP32 | GPIO22 | I²C clock line (shared with display and BMS). |
| INTA | ESP32 | <mark>GPIO (TBD)</mark> | Interrupt output for Port A changes; 4.7 kΩ pull-up to 3.3V. |
| INTB | ESP32 | <mark>GPIO (TBD)</mark> | Interrupt output for Port B changes; 4.7 kΩ pull-up to 3.3V. |
| RESET | 3.3V | — | Active-low reset; tied to 3.3V to keep device enabled. |
| A0 | <mark>TBD</mark> | — | I²C address bit 0. |
| A1 | <mark>TBD</mark> | — | I²C address bit 1. |
| A2 | <mark>TBD</mark> | — | I²C address bit 2. |
| GPA0 | BTN_BACK | — | Back button input (filtered, active-low). |
| GPA1 | BTN_TOP | — | Up button input (filtered, active-low). |
| GPA2 | BTN_UI | — | UI button input (filtered, active-low). |
| GPA3 | BTN_LEFT | — | Left button input (filtered, active-low). |
| GPA4 | BTN_OK | — | OK button input (filtered, active-low). |
| GPA5 | BTN_RIGHT | — | Right button input (filtered, active-low). |
| GPA6 | BTN_SETTINGS | — | Settings button input (filtered, active-low). |
| GPA7 | BTN_DOWN | — | Down button input (filtered, active-low). |
| GPB0 | BTN_DBG | — | Debug button input (filtered, active-low). |
| GPB1-GPB7 | Unused | — | Available for future expansion. |
| VSS | GND | GND | Ground. |
| VDD | 3.3V | 3.3V | Power supply (1.8-5.5V). |

### Touch Board Based On MPR121QR2 Touch Controller

#### Description

The MPR121QR2 is a 12-channel capacitive touch sensor controller used to implement a touch-sensitive electrode matrix. It communicates with the ESP32 via I2C at address <mark>I2C_ADDRESS_TBD</mark>. The device provides interrupt functionality through its NOT IRQ pin, allowing the ESP32 to detect touch events efficiently.

The electrode matrix follows the design from [NXP Application Note AN4600](https://www.nxp.com/docs/en/application-note/AN4600.pdf), where touch detection is achieved by the intersection of X and Y electrodes:
- ELE0-ELE6 correspond to X0-X6 positions (7 columns)
- ELE7-ELE11 correspond to Y0-Y4 positions (5 rows)

Touch coordinates are determined by the intersection of active XN and YM electrodes.

#### Configurations

Hardware:

1. I2C address set via ADD pin: <mark>ADDRESS_CONFIG_TBD</mark>
2. NOT IRQ pin pulled up to 3.3V through 4.7 kΩ resistor
3. REXT pin pulled down through 75 kΩ resistor to GND
4. 0.1 µF decoupling capacitor between VDD and VSS
5. 0.1 µF capacitor between VREG and GND for internal regulator stability
6. PCB electrode layout follows AN4600 routing guidelines
7. Electrode pad size: 5 mm × 5 mm (increased from AN4600's 3.5-4 mm recommendation)

Software:

1. <mark>Configure touch and release thresholds for each electrode</mark>
2. <mark>Enable electrode matrix scanning mode according to AN4600</mark>
3. <mark>Implement touch coordinate decoding from electrode intersections</mark>

#### Pinout

| Pin / Signal | Connection | Net / GPIO | Description |
|-------------|------------|------------|-------------|
| SDA | ESP32 | GPIO21 | I²C data line (shared with display, BMS, and button board). |
| SCL | ESP32 | GPIO22 | I²C clock line (shared with display, BMS, and button board). |
| IRQ | ESP32 | <mark>GPIO (TBD)</mark> | Active-low interrupt output for touch events; 4.7 kΩ pull-up to 3.3V. |
| ADD | <mark>TBD</mark> | — | I²C address selection pin. |
| ELE0 | Touch electrode | X0 | Electrode column 0. |
| ELE1 | Touch electrode | X1 | Electrode column 1. |
| ELE2 | Touch electrode | X2 | Electrode column 2. |
| ELE3 | Touch electrode | X3 | Electrode column 3. |
| ELE4 | Touch electrode | X4 | Electrode column 4. |
| ELE5 | Touch electrode | X5 | Electrode column 5. |
| ELE6 | Touch electrode | X6 | Electrode column 6. |
| ELE7 | Touch electrode | Y0 | Electrode row 0. |
| ELE8 | Touch electrode | Y1 | Electrode row 1. |
| ELE9 | Touch electrode | Y2 | Electrode row 2. |
| ELE10 | Touch electrode | Y3 | Electrode row 3. |
| ELE11 | Touch electrode | Y4 | Electrode row 4. |
| REXT | 75 kΩ → GND | — | External resistor for charge current setting. |
| VREG | 0.1 µF cap → GND | — | Internal regulator output; decoupled to GND. |
| VSS | GND | GND | Ground. |
| VDD | 3.3V | 3.3V | Power supply (1.71-3.6V); 0.1 µF decoupling to GND. |

#### Links

[Application Note AN4600 - MPR121 Proximity Sensing](https://www.nxp.com/docs/en/application-note/AN4600.pdf)


## Software

### Assets

#### Description

There will be 4 main face assets libraries(profiles) where each of them can be set as source library, single library with faces not
connected to any library and few non-main libraries which
may be added in future for Bobot:

1. Classic - face library of first Bobot with possible extensions for new features
2. Amethyst - face library for current model which will stick to classic expressions but with fresh look
3. Cutie - face library consisting of cute assets with cat-like features and big eyes
4. Doodles - face library consisting of weirdly paint assets representing scratch style
4. Other - library with face expressions not connected to any full library, cannot be set as source library

#### Structure

##### Expression

Expression - elementary unit of Bobot expressions graphic. It is directory with graphic assets of expression nd file with expression description and metadata. Expression can be set as temporally Bobot face in mode with manual expression selection.

Expression strucutre:

```
Expression
├── Description.ini
├── Frames
│   ├── Frame_00.bin
│   ├── Frame_01.bin
│   ├──...
│   ├── Frame_N.bin
```

Frame binary format:
Each frame file is binary file with monochrome bitmap data optimized for u8g2 library. Frame dimensions are stored in Description.ini to reduce file size and improve loading speed. Format structure: monochrome bitmap data where each byte represents 8 vertical pixels organized in column-major order. During runtime dimensions are read once from Description.ini, then frames are read from SD card and passed directly to u8g2 display functions.

Expression `Description.ini`:
```ini
; Description for Expression_Name expression

[Loop]
; Supported Loop types:
;
; 1. IdleBlink(default) - Display first frame for random time in range
;                           [IdleTimeMinMS, IdleTimeMaxMS] ms and after that
;                           run animation with AnimationFPS fps.
;                           After that switch frame to first and repeat.
;
; 2. Loop - Repeat animation from first frame to last with AnimationFPS fps.
;           IdleTimeMinMS and IdleTimeMaxMS are unnecessary and wiil be ignored
;           if presented
;
; 3. Image - Display only first frame. All fields except type are unnecessary 
;             and wiil be ignored if presented

; Type field must be first
Type = IdleBlink ; default = IdleBlink
AnimationFPS = 20 ; default = 20
IdleTimeMinMS = 1000 ; default = 1000
IdleTimeMaxMS = 1000 ; default = 1000

[Dimensions]
; Frame dimensions (auto-filled by export tool)
Width = 128
Height = 64
```

##### Library
Library - list of expressions. Some libraries can be sourced to be used as Bobot faces for corresponding events. Library consist of file with library description, information and metadata, directory with expressions and file with behaviour and mapping(if needed) expressions in expressions directory to Bobots events. If there is no behaviour and mapping file, library cant be sourced.
```
Library
├── Description.ini
├── Behaviour.csv
├── Expressions
│   ├── Expression_A
│   ├── Expression_B
│   ├──...
│   ├── Expression_Z
```

### Developer Tools

#### Graphics Structure Generation Script
Script is placed in `devtools` directory under project root. It consists of two files: `generate_graphics_structure.py` with main logic and `generate_graphics.sh` shell wrapper for convenient execution. Script can be run with `./devtools/generate_graphics.sh` from project root.

Script reads `assets/graphics/Description.ini` and processes all libraries marked as `true`. For each enabled library it creates directory structure in `assets/graphics/libraries/LibraryName` if not exists. Then it iterates over all directories inside library which are expressions. For each expression it generates default `Description.ini` if not present.

For expressions containing `.aseprite` or `.ase` files script uses aseprite in batch mode to export animation frames as PNG sequence into temporary directory. Then it converts each PNG frame to u8g2-compatible binary format and saves as `Frame_XX.bin` files in expression `Frames` subdirectory. After conversion all temporary PNG files are deleted. If `Frames` directory exists from previous run it is completely cleared before generating new frames to avoid stale data.

Aseprite is built from source during Docker container setup and available at `/usr/local/bin/aseprite`. Script automatically finds aseprite executable and falls back gracefully if not available.

#### SD Card Assets Update Script
**WARNING: Currently this feature is broken and dont work properly**

##### ESP32 Side
ESP32 can enter asset upload mode when requested, hosting a simple HTTP server on WiFi. In main microcontroller loop it checks for button combination or serial command to activate upload mode. When activated, ESP32:
- Stops regular operating mode
- Starts HTTP server on port 8080
- Waits for upload requests

Upload process:
1. HTTP POST to `/start` - ESP32 deletes entire `/assets` directory (preserving other files/directories)
2. HTTP POST to `/file` - Upload individual files with path in request
3. HTTP POST to `/complete` - Finalize upload and return to normal mode

The `/assets` directory is completely cleared at start of upload, but other directories and files on SD card remain untouched. Upload mode cannot be interrupted except by system-level events.

##### Host (PC) Side
Host PC connects to ESP32 via WiFi to upload assets. In `devtools` directory there are `flash_micro_sd.sh` and `flash_micro_sd.py` scripts:

**Shell script (`flash_micro_sd.sh`)**:
- Manages directory navigation
- Configures Python environment
- Provides simple interface: `./devtools/flash_micro_sd.sh`

**Python script (`flash_micro_sd.py`)**:
- Discovers ESP32 on network (mDNS or manual IP)
- Sends `/start` command and waits for confirmation
- Uploads entire `assets` directory via HTTP POST (excludes .aseprite files)
- Sends `/complete` command when done
- Displays progress and handles errors

Usage:
```bash
# From project root
./devtools/flash_micro_sd.sh

# Or with custom IP
./devtools/flash_micro_sd.sh --ip 192.168.1.100
```

The upload is reliable (checksums verified), fast (WiFi bandwidth), and doesn't interfere with USB console/debugging.

### Graphic Engine

#### Position class Vec2i

Class which will hold two integer values x and y and whcih will be used for working with position values.
This class will have implementation of basic operaions: addition, substraction, multiplicating, their with assignment analogues(e.g. plus equal) and negative sign.
Also it will have functions which will return string that represents vector data.
This class will be used to hold values like positions, offset, speed, etc.

#### Frame class

Simple class which will hold only bitmap value of frame in format which will be place-and-go for u8g2 function to draw bitmap on screen and frame size.
Class will have function to build object from raw file data, received from storage through DMA.

#### Expression class

Class which will hold sequence of frames, current frame index, fps and data about correct playback way of animation.
This class will have functions to draw current frame on given display with given offset, increment frame index, set frame index, update function which will handle correct index incrementation depended on animation loop type.
Also there will be function to create animation object(which will do subcalls of constructing frames from files) from given directory

#### Future functional
For now this code is just to test code on development setup to understand that prom not in code, when test will be runned on manufactured pcb's. They wont be implemented right now for time saving and simplisity.
Future functional may include, but not limited to:

1) Expression Library class which will handle mapping events/states to storage directories and will handle dynamic load of expression on event/state switch(to not hold full expressions library in memory). Also this class can handle graphic profiles switch.

2) Upgrading Expression class functions to work correct in energy efficient code, including handling fps using timer interrupts, to set esp32 to sleep mode for as long as possible.

3) Profile class which will be placeholder for data about each library possibilities, storage path and names.

4) Classes for text, side menu, burger menu, navigation system visualizations, regular bitmaps drawings, and possibly more.
