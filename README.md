# Bobot_2_Amethyst
Second robot from Bobot series of robots. A huge amount of things in this robot address problems of previous Bobot.

# Features 

## Mechanical

### Handle

#### Description

On the top there is the handle which is hidden in base.
It rotates out on bearings.
Rotation is locked by mechanism on top of base.

### Back buttons

#### Description

On the back there are 9 buttons to interact with bobot.
They are with 3d printed tops with symbols representing purpose.

Buttons:
```
 ______________________________
|   Back   |  Up  |  Switch UI |
|__________|______|____________|
|   Left   |  OK  |   Right    |
|__________|______|____________|
| Settings | Down |    Text    |
|__________|______|____________|
```

### Power switch

#### Description

At the bottom of left side of bobot(looking at their face) there is power switch, which is connect batteries to power regulation module.

### Screwdriver with built in case

#### Description

At the left side of bobot there is case with screwdriver which can unscrew all screws in bobot.
It is based on push latch mechanism, to be held inside of bobot.

### Access panel

#### Description

At the right side there is panel on 2 screws which give access to main microcontroller esp32 dev kit board. When this panel taken apart, esp32 can be fully removed from socket and access to micro sd is revealed.

## Hardware

### ESP32 Microcontroller

#### Description

Main microcontroller will be unnamed clone of esp32 wroom dev kit with `30 pins`.
It will be placed in sockets on pcb on right side of bobot.

#### Links

[Overview](https://www.espboards.dev/esp32/esp32-30pin-devkit-generic/)

[Datasheet](https://documentation.espressif.com/esp32_datasheet_en.pdf)

[Technical Reference Manual](http://documentation.espressif.com/esp32_technical_reference_manual_en.pdf)

### 2.42 inch OLED I2C display

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

### Li-po 3000mAh 3.7V battery 1S2P

#### Description

There are two parallely connecteed li-po battery in bobot, which in total give 6000mAh battery capacity.

#### Links

[Buy](https://prom.ua/ua/p2843472521-litij-polimernyj-akkumulyator.html)

### BQ25895 Battery management module

#### Description

Bobot have power management module, which will consist of: 
- `BQ25895` I2C Controlled Single Cell 5-A Fast Charger with MaxChargeTM for High Input
Voltage and Adjustable Voltage 3.1-A Boost Operation
- <mark>Thermistor as BQ25895 requires
- Connection to esp32 i2c bus

#### Pinout

##### Activelly used

| Pin / Signal | Connection | Net / GPIO | Description |
|-------------|------------|------------|-------------|
| VBUS | USB Type-C port | VBUS | Charger input supply (3.9–14 V). Decouple close to pin. |
| PGND | Power ground | GND | High-current ground return for charger, battery, SYS, PMID caps. |
| SDA | ESP32 | GPIO21 | I²C data line for charger configuration/monitoring. |
| SCL | ESP32 | GPIO22 | I²C clock line for charger configuration/monitoring. |
| INT | ESP32 | GPIO (TBD) | Open-drain interrupt (active-low) for charge/fault events; 10 kΩ pull-up to 3.3 V. |
| STAT | Status LED | — | Open-drain charge status output; LED + resistor to 3.3 V. |
| CE | GND | — | Charge enable (active-low); tied to GND for always-enabled charging. |
| ILIM | Resistor → GND | R_ILIM | Hardware input-current limit set to ~3 A (protects USB source). |
| TS | NTC divider | NTC | Battery temperature sense via NTC near battery; safety-critical. |
| BAT | Battery pack | BAT+ | Battery positive terminal (1S Li-ion/Li-poly). |
| SYS | System rail | SYS | Managed system supply; feeds 3.3 V regulator for ESP32. |
| SW | Inductor | SW | Switching node to inductor (keep short, noisy). |
| BTST | Bootstrap cap | BTST | Bootstrap capacitor to SW for high-side gate drive. |
| REGN | Decoupling cap | REGN | Internal LDO output; bias rail (decouple to GND). |
| EPAD (PowerPAD) | PCB copper | PGND | Thermal + electrical ground; must be soldered to PGND with vias. | 
| OTG | Disabled | Tie to GND | Prevents boost (OTG) mode activation. |
| PMID | Not exported | Local ceramic caps to PGND only (≥8.2 µF) | Do not power loads; keep short return. |
| QON | Unused | Leave unconnected | Internal pull-up keeps default behavior. |
| DSEL | Unused | Leave unconnected | Not needed without external USB data switch. |
| D+ | USB only | Connect only to USB connector | Used internally for BC1.2/MaxCharge detection. |
| D− | USB only | Connect only to USB connector | Same as D+. |

#### Links

[BQ25895 datasheet](https://www.ti.com/lit/ds/symlink/bq25895.pdf)

[Buy BQ25895](https://www.aliexpress.com/item/1005010544702640.html)