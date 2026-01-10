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

There will be 4 main face assets libraries where each of them can be set as source library, single library with faces not
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
```
Expression
├── Description.ini
├── Frames
│   ├── Frame_00
│   ├── Frame_01
│   ├──...
│   ├── Frame_N
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
