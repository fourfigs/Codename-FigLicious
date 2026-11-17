# Codename-FigLicious

A GNOME HOME rebrand concept for the Piglet wardriver.

## Touchscreen preview

Open [dashboard-preview/index.html](dashboard-preview/index.html) in a desktop or mobile browser. The interface is laid out at the target display's 480×320 landscape ratio and scales to fit the browser window.

- **Home:** Miss Direction frames the GPS readout. Use the GPS PRESENT switch above the screen to preview receiver-present and no-fix states.
- **Ward Ride:** Tap Lady Dualbandit to open the field interface. Its horizontally scrollable tabs include Hunt, Radar, Finds, Security, Channel, Spectrum, Field, Tools, Alerts, GPS, Privacy, and Transfer.
- **Hunt:** Filter All, Wi-Fi, BLE, Classic Bluetooth, Cellular, Mesh, or ADS-B; sort by RSSI, recency, sightings, or name. Contact rows show signal tiers and sighting counts. Tap a row for details; long-press it to open Foxhunt.
- **Foxhunt:** Follow sample RSSI as a hot/warm/cold cue and toggle the beeper or silence. The browser preview does not drive a physical beeper.
- **Radar / Finds:** Preview live proximity tiers and the historical finds map.
- **Security / Channel / Spectrum:** Review sample encryption and channel counts, plus a sample spectrum trace. Handshake/PMKID capture is not enabled.
- **Field / Tools / Alerts:** Browse manufacturer and device classes, Remote ID, mesh/ADS-B/cellular hardware status, and heuristic alert categories.
- **GPS:** View sample fix and satellite diagnostics; the home screen shows Miss Direction only when GPS is present.
- **Privacy:** Preview _nomap/_optout, blacklist, and exclusion-zone controls.
- **Transfer:** Preview CSV import/export and queued upload actions.
- **SD Card:** Tap Cache Cassidy to browse the sample log directory and file list. These entries are demonstration data.

The preview is informed by the supplied GNOME HOME loading-screen reference and Wardrive-Go’s feature organization. Its vector portraits are concept stand-ins; replace them with optimized crops/sprites from the source artwork before the firmware port.

## Firmware integration status

This is a browser-based UI prototype. It does not yet read live GPS, scan counters, or SD contents, and it is not a flashable firmware build.

Piglet v3.06 currently uses a 128×64 monochrome SSD1306 interface. Bringing this 480×320 SPI ST7796S display online requires a TFT display backend, a resistive-touch driver and calibration, and verified wiring for the specific display and target board. The firmware pin map is intentionally still open.

The ESP32-C5 has dual-band Wi-Fi (2.4/5 GHz) and Bluetooth LE. Classic Bluetooth is unsupported by the chip. Cellular and ADS-B require separate receivers; Meshtastic/MeshCore node scanning requires a compatible sub-GHz radio. The prototype keeps these categories visible and marks absent hardware instead of inventing live results.

## References

- Piglet v3.06: <https://github.com/Hamspiced/piglet/releases/tag/v3.06>
- Wardrive-Go: <https://github.com/RocketGod-git/wardrive-go>

The prototype is independently written and does not copy either project's firmware or Android source. Piglet v3.06 is distributed under CC BY-NC-SA 4.0; Wardrive-Go is distributed under GPL-3.0. Review their terms before incorporating upstream code. This repository retains its existing MPL-2.0 license.
