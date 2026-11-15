# FigLicious GNOME HOME Wardriver

I am building this firmware for a Seeed XIAO ESP32-C5 with a 480×320 ST7796S SPI display, XPT2046 resistive touch, microSD storage, and a UART GPS receiver. The firmware uses only observations reported by connected hardware. A missing GPS fix, SD card, or radio receiver is shown as unavailable instead of being replaced with sample data.

I keep the browser dashboard prototype at [dashboard-preview/index.html](dashboard-preview/index.html). Its vector portraits are concept stand-ins; I will replace them with the supplied character artwork when I prepare the final artwork assets.

## Radio-use confirmation

The radio interface is designed for lab use only. Before I start a radio scan, I must check the box and press **Confirm**:

> I confirm I will use this device only in a lab or on systems I am authorized to assess. This firmware performs receive-only scans.

The confirmation is saved on the device. Scanning stays locked until I confirm. Wi-Fi deauthentication, spoofing, and packet injection are not included.

## Hardware pinout

I preserve the XIAO ESP32-C5's normal SPI pins from the pinout diagram: D8/GPIO8 SCK, D9/GPIO9 MISO, and D10/GPIO10 MOSI. The display, touch controller, and SD card share those SPI signals and each uses a separate chip-select line.

| Connection | XIAO ESP32-C5 | Module pin | Notes |
| --- | --- | --- | --- |
| SPI clock | D8 / GPIO8 | TFT SCK, touch T_CLK, SD SCK | Shared SPI clock |
| SPI controller input | D9 / GPIO9 | TFT SDO, touch T_DO, SD MISO | Shared MISO |
| SPI controller output | D10 / GPIO10 | TFT SDI, touch T_DIN, SD MOSI | Shared MOSI |
| TFT chip select | D2 / GPIO25 | TFT CS | Dedicated display select |
| TFT data/command | D3 / GPIO7 | TFT DC/RS | Dedicated display control |
| Touch chip select | D6 / GPIO11 | Touch T_CS | XPT2046 select; touch IRQ is left unconnected and polled |
| SD chip select | D4 / GPIO23 | SD_CS | Dedicated SD select; use the display board's SD_CS pad if using its onboard socket |
| GPS receive | D7 / GPIO12 (RX) | GPS TX | GPS TX is the only GPS data wire connected; leave GPS RX unconnected |
| Ground | GND | GND | Common ground |
| Module power | 3V3 or 5V per the board selector | VCC | Follow the display board's J1 voltage-selector marking before applying power |

The XIAO pinout image also assigns D0/GPIO1 to Ebyte CE, D1/GPIO0 to Ebyte CSN, and D5/GPIO24 to CC-CS; those assignments are preserved. I have not assigned an actuator pin. Beep and vibration choices remain unavailable until I identify the connected hardware and its wiring.

I check the display breakout's silkscreen and J1 setting before connecting power. The available photos identify the board family and its 480×320 SPI interface, but do not clearly establish the jumper setting or every connector label. I leave TFT reset disconnected from the XIAO and hold the display reset line inactive as documented for the breakout; I do not connect a signal to a pin whose function is unclear. I confirm the breakout's exact pin labels against its own silkscreen before powering it.

The assignments use the GPIOs shown in my XIAO C5 design reference and do not use the GPIO26/27/28 boot-mode strap group. GPIO25 and GPIO7 are also boot-time sampled pins on the C5 family; the display's CS/DC inputs must not drive them to an unintended level during reset. I verify the breakout input bias and startup behavior before treating boot reliability as hardware-tested.

## Firmware build

I build the C5 firmware with PlatformIO:

```sh
pio run -e wardriver
```

The resulting application image is `.pio/build/wardriver/firmware.bin`. To flash from PlatformIO when the XIAO is connected:

```sh
pio run -e wardriver -t upload
```

I do not call an image flashable until the build completes successfully. I have not tested this firmware on connected hardware.

## `libnet80211.a` status

I keep the supplied `updated-lib/libnet80211.a` in the project as an artifact to investigate. It is an archive of RISC-V objects, but matching the C5 architecture alone does not establish SDK, compiler, or ABI compatibility. I do not link it into this build until those compatibility checks succeed. It is not a drop-in Arduino Wi-Fi API. The current firmware uses the installed ESP32 Arduino Wi-Fi receive/scanning interfaces and does not enable transmit attacks.
