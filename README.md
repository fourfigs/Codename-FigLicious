# Codename-FigLicious

A GNOME HOME rebrand concept for the Piglet wardriver.

## Touchscreen preview

Open [dashboard-preview/index.html](dashboard-preview/index.html) in a desktop or mobile browser. The interface is laid out at the target display's 480×320 landscape ratio and scales to fit the browser window.

- **Home:** Miss Direction frames the GPS readout. Use the GPS PRESENT switch above the screen to preview receiver-present and no-fix states.
- **Ward Ride:** Tap Lady Dualbandit to open five views: Hunt, Security, Field, Tools, and Alerts.
- **Hunt:** Filter All, Wi-Fi, BLE, Classic Bluetooth, Cellular, Mesh, or ADS-B; sort by RSSI, recency, sightings, or name. Each row shows a signal tier and last-seen context. Long-press a contact to open Foxhunt.
- **Foxhunt:** Follow the sample RSSI as a hot/warm/cold cue and toggle the beeper or silence. The browser preview does not drive a physical beeper.
- **Security:** Review encryption mix, frequency bands/channels, frame categories, and capture status. Handshake/PMKID capture is not enabled.
- **Field:** Drill into vendor/OUI, device classes, and first-time versus repeat sightings.
- **Tools:** Includes Remote ID, mesh, ADS-B, and cellular cards. Categories that need hardware beyond the C5 are marked as requiring a receiver.
- **Alerts:** Groups camera/infrastructure, tracker, public-safety RF, test-gear, and wearable matches. The sample view labels these as heuristic and asks for verification.
- **SD Card:** Tap Cache Cassidy to browse the sample log directory and file list. These entries are demonstration data.

The preview uses the GNOME HOME palette and character roles from the supplied loading-screen reference. Its vector portraits are concept stand-ins; replace them with optimized crops/sprites from the source artwork before the firmware port.

## Firmware integration status

This is a browser-based UI prototype. It does not yet read live GPS, scan counters, or SD contents, and it is not a flashable firmware build.

Piglet v3.06 currently uses a 128×64 monochrome SSD1306 interface. Bringing this 480×320 SPI ST7796S display online requires a TFT display backend, a resistive-touch driver and calibration, and verified wiring for the specific display and target board. The firmware pin map is intentionally still open.

The ESP32-C5 has dual-band Wi-Fi (2.4/5 GHz) and Bluetooth LE. Classic Bluetooth is unsupported by the chip. Cellular and ADS-B require separate receivers; Meshtastic/MeshCore node scanning requires a compatible sub-GHz radio. The prototype keeps these categories visible and marks absent hardware instead of inventing live results.

## Upstream reference

Piglet v3.06: <https://github.com/Hamspiced/piglet/releases/tag/v3.06>

This prototype is independently written and does not copy Piglet firmware source. Piglet v3.06 is distributed under CC BY-NC-SA 4.0; review its terms before incorporating or distributing any upstream code. This repository retains its existing MPL-2.0 license.
