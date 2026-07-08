# ESP32 Dual Band Wardriver
Based on the ESP32-C5-DevKitC-1 v1.2, the ESP32 Dual Band Wardriver offers wardriving capabilities for 2.4GHz and 5GHz WiFi as well as BLE.
Logs are formatted for WiGLE and saved to SD card.

## Table of Contents
- [Leaderboards](#leaderboards)
- [Connections](#connections)
    - [Display](#display)
    - [GPS](#gps)
    - [SD Card](#sd-card)
    - [Battery Fuel Gauge](#battery-fuel-gauge)
    - [Activity LED](#activity-led)
    - [User Buttons](#user-buttons)
- [Install Firmware](#install-firmware)
- [Update Firmware](#update-firmware)
- [Usage](#usage)
    - [Booting](#booting)
    - [Initial Setup](#initial-setup)
    - [Webserver Usage](#webserver-usage)
    - [Display Screens](#display-screens)
    - [Buttons](#buttons)
    - [Uploads](#uploads)
    - [Dock Mode](#dock-mode)
    - [Geofence Zones](#geofence-zones)
    - [SSID Exclusions](#ssid-exclusions)
    - [Debug Logging](#debug-logging)
- [Modes](#modes)

## Leaderboards
Join **#wardriving** on [WiGLE](https://wigle.net/stats#groupstats) and **KokosStripClub (invite code: _mN01r0TAS8q)** on [WDGWars](https://wdgwars.pl) to have a little competitive fun with the art of wardriving.

## Connections
**IMPORTANT: If you are using the ESP32-C5-DevKitC-1 with the JCMK C5 Wardriver host board or you are powering your DevKit via the 3V3 pin, you must remove the [3V3 jumper](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c5/esp32-c5-devkitc-1/user_guide.html#current-measurement) from the DevKit or your device will not power properly.**

### [Display](https://a.co/d/dO8M3Ec)
| ESP32-C5 | Display |
| -------- | ------- |
| `3V3`    | `VCC`   |
| `GND`    | `GND`   |
| `GPIO6`  | `SCK`   |
| `GPIO7`  | `MOSI`  |
| `GPIO23` | `CS`    |
| `GPIO24` | `DC`    |
| `GPIO27` | `BL`    |
| `RST`    | `RST`   |

### [GPS](https://a.co/d/hIqIitg)
| ESP32-C5 | GPS   |
| -------- | ----- |
| `3V3`    | `VCC` |
| `GND`    | `GND` |
| `GPIO13` | `RX`  |
| `GPIO14` | `TX`  |

> **Tip:** The GPS module's ceramic patch antenna should be oriented face-up toward the sky for best signal. In the JCMK host board case the antenna mounts vertically by default — relocating it to the top of the enclosure significantly improves acquisition time.

### [SD Card](https://www.sparkfun.com/sparkfun-microsd-transflash-breakout.html)
| ESP32-C5 | SD     |
| -------- | ------ |
| `3V3`    | `VCC`  |
| `GND`    | `GND`  |
| `GPIO6`  | `SCK`  |
| `GPIO2`  | `MISO` |
| `GPIO7`  | `MOSI` |
| `GPIO10` | `CS`   |

### [Battery Fuel Gauge](https://www.adafruit.com/product/5580)
| Board       | ESP32-C5 | Pin   |
| ----------- | -------- | ----- |
|             | `GPIO4`  | `SCL` |
|             | `GPIO5`  | `SDA` |
|             | `3V3`    | `VCC` |
|             | `GND`    | `GND` |
| `Battery +` |          | `BAT` |

### Activity LED
| ESP32-C5 | LED |
| -------- | --- |
| `GPIO28` | `+` |
| `GND`    | `-` |

### User Buttons
The User Buttons require pull-down resistors.
| ESP32-C5 | Button   |
| -------- | -------- |
| `GPIO8`  | `DOWN`   |
| `GPIO9`  | `UP`     |
| `GPIO15` | `SELECT` |

## Install Firmware
1. Clone this repo
2. In your workstation CLI, navigate to the `C5_Py_Flasher` directory
3. With your ESP32-C5 device unplugged, execute `python c5_flasher.py` and allow any missing python packages to install
4. Once you see `Waiting for ESP32-C5 device to be connected...`, connect your ESP32-C5 device to your PC via USB-C cable
5. Once you see `Ready to flash these files to ESP32-C5? (y/N):`, enter `y` and allow the firmware to flash
6. When the `Hardware reset` message appears on the screen, you may disconnect your ESP32-C5 device

> **Note:** The ESP32-C5 DevKit must be removed from the JCMK host board before flashing. The host board's circuitry prevents the DevKit from enumerating over USB while seated.

## Update Firmware
The firmware is designed to check the SD card root at every boot for a new `.bin` file. If a new bin file is found, it automatically executes an update.

1. Download the latest firmware from [Releases](../../releases)
2. Place the `.bin` file on the root of your SD card
3. Install the SD card into the C5 Wardriver
4. Boot the C5 Wardriver and allow the automatic update process to execute

## Usage

### Booting
When powered on, the C5 Wardriver attempts to connect to WiFi using saved credentials. If no credentials are saved or the network is unavailable, it starts its own access point named `c5wardriver` (password: `c5wardriver`). The AP remains active for 60 seconds if no clients connect, or until all clients disconnect.

If the device connects to a saved WiFi network, the web UI remains available indefinitely — it will stay connected until the network disappears. You can skip the admin phase entirely by holding `SELECT` when the boot logo appears.

### Initial Setup
On first boot, connect to the `c5wardriver` access point and navigate to `http://192.168.4.1`. From the web UI you can configure:

- **WiFi SSID / Password** — network to connect at boot for web UI access
- **WiGLE API credentials** — for direct log upload to WiGLE
- **WDG Wars API key** — for direct log upload to WDGWars
- **Trigger SSID / Password** — network that triggers dock mode (see [Dock Mode](#dock-mode))
- **Admin Password** — enables Basic Auth on the web UI
- **SSID Exclusions** — SSIDs to never log
- **Geofence Zones** — zones where wardriving pauses
- **SD Debug Log** — enable to write all log entries to `/debug.log` on the SD card

### Webserver Usage
At every boot, if connected to a saved WiFi network, the device IP is shown on the display. Navigate to that IP from any device on the same network to access the web UI. From here you can download log files, upload to WiGLE or WDGWars, and reconfigure all settings.

A live log viewer is available at `http://<device-ip>/log` — auto-refreshes every 5 seconds and shows the last 100 log entries.

### Display Screens
Cycle between screens using the `UP` and `DOWN` buttons.

**Screen 1 — Stats (default)**
Large-format wardriving stats: GPS satellite count and lock status, battery percentage, scan status, current session 2.4GHz / 5GHz / BLE counts, running totals, and active geofence zone name.

**Screen 2 — Detail**
Original stats display: firmware version, SD status, battery, scan status, log file name, per-band counts, GPS satellites, and running totals.

**Screen 3 — Incognito**
5-second countdown then backlight off. Press any button to exit.

### Buttons
| Button   | Function |
| -------- | -------- |
| `UP`     | Cycle display mode forward |
| `DOWN`   | Cycle display mode backward |
| `SELECT` | Hold at boot logo to skip admin phase |

### Uploads
Log files can be uploaded to WiGLE and WDGWars directly from the web UI or automatically via dock mode. Select a log file from the file list and choose Upload — options are WiGLE, WDGWars, or Both.

Sidecar files (`.wigle` / `.wdg`) are created after successful uploads to prevent duplicate uploads across reboots.

### Dock Mode
Dock mode automates log uploads when the wardriver detects a configured trigger SSID.

**How it works:**
1. While wardriving or in standby, the device passively scans for the trigger SSID every 30 seconds
2. When detected, it connects and uploads all pending log files (files without upload sidecars)
3. It monitors for the trigger SSID to disappear, then resumes wardriving automatically

**Two-tier operation:**
- **Tier 1 (no GPS fix):** Connects and serves the web UI only — no upload triggered
- **Tier 2 (GPS fix + SD card):** Full dock mode — uploads all pending logs to WiGLE and WDGWars

Tier 1 automatically upgrades to Tier 2 if a GPS fix is acquired while docked.

Configure the trigger SSID and password in the web UI under **Dock Mode**. The trigger SSID is separate from the boot WiFi network.

### Geofence Zones
Up to 5 geofence zones can be configured with a label, latitude/longitude, and radius (0.10 to 1.00 miles). When the wardriver enters a geofence zone, wardriving pauses and the zone name is shown on the display. Wardriving resumes automatically when the zone is exited.

Zones are configured from the web UI under **Geofences**.

### SSID Exclusions
Up to 10 SSIDs can be added to the exclusion list. Networks matching an excluded SSID are never logged regardless of location. Useful for filtering out your own networks or known networks you don't want to record.

Configure from the web UI under **SSID Exclusions**.

### Debug Logging
Enable **SD Debug Log** in the web UI Admin section to write all log entries to `/debug.log` on the SD card. Useful for diagnosing issues in the field without a serial connection. The debug log is available for download from the web UI file list but is excluded from upload queues.

## Modes
The C5 Wardriver supports multiple ESP32 modules operating as collection nodes reporting to a single Core device. Nodes and Cores communicate over-the-air within typical WiFi range.

**When configuring device mode from the web UI, the mode persists across reboots. When selecting mode from the on-device menu, it is temporary and resets on next boot.**

### Solo Mode
Standalone operation. Requires SD card, GPS module, and GPS fix. Logs directly to SD and supports direct upload to WiGLE and WDGWars.

### Node Mode
Collects wardriving data and sends it to a Core device. Does not require SD card or GPS.

### Core Mode
Receives data from Nodes, pairs it with GPS location data, and saves consolidated log files to SD. Requires SD card and GPS. Does not collect wardriving data itself. Supports direct upload to WiGLE and WDGWars.

### Encryption
Node-to-Core communications can be encrypted. Enable via the web UI. When encryption is enabled, a maximum of 6 nodes can operate simultaneously due to memory constraints. Without encryption, the number of nodes is theoretically unlimited.
