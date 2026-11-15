# Codename-FigLicious

A GNOME HOME rebrand concept for the Piglet wardriver.

## Touchscreen preview

Open [dashboard-preview/index.html](dashboard-preview/index.html) in a desktop or mobile browser. The interface is laid out at the target display's 480×320 landscape ratio and scales to fit the browser window.

- **Home:** Miss Direction frames the GPS readout. Use the GPS PRESENT switch above the screen to preview receiver-present and no-fix states.
- **Ward Ride:** Tap Lady Dualbandit to open the scanning view. The scan button toggles the screen state.
- **SD Card:** Tap Cache Cassidy to browse the sample log directory and file list. These entries are demonstration data.

The portrait drawings and GNOME HOME color system are concept artwork based on the supplied loading-screen reference. Replace the vector portraits with optimized crops/sprites from the source artwork when that image is available as a repository asset.

## Firmware integration status

This is a browser-based UI prototype. It does not yet read live GPS, scan counters, or SD contents, and it is not a flashable firmware build.

Piglet v3.06 currently uses a 128×64 monochrome SSD1306 interface. Bringing this 480×320 SPI ST7796S display online requires a TFT display backend, a resistive-touch driver and calibration, and the exact display/touch wiring for the target board. The current prototype keeps those hardware assignments out of the UI until they are verified.

## Upstream reference

Piglet v3.06: <https://github.com/Hamspiced/piglet/releases/tag/v3.06>

This prototype is independently written and does not copy Piglet firmware source. Piglet v3.06 is distributed under CC BY-NC-SA 4.0; review its terms before incorporating or distributing any upstream code. This repository retains its existing MPL-2.0 license.
