# ESP32 Dual Band Wardriver
Based on the ESP32-C5-DevKitC-1 v1.2, the ESP32 Dual Band Wardriver offers wardriving capabilities for 2.4GHz and 5GHz WiFi as well as BLE.
Logs are formatted for Wigle and saved to SD card.

## Table of Continents
- [Connections](#connections)
    - [Display](#display)
    - [GPS](#gps)
    - [SD Card](#sd-card)
    - [Activity LED](#activity-led)
    - [User Buttons](#user-buttons)
- [Install Firmware](#install-firmware)
- [Update Firmware](#update-firmware)
- [Usage](#usage)

## Connections
**IMPORTANT: If you are using the ESP32-C5-DevKitC-1 with the JCMK C5 Wardriver host board or you are powering your DevKit via the 3V3 pin, you much remove the [3V3 jumper](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c5/esp32-c5-devkitc-1/user_guide.html#current-measurement) from the DevKit or your device will not power properly**

### [Display](https://a.co/d/dO8M3Ec)
| ESP32-C5 | Display |
| -------- | -- |
| `3V3`    | `VCC` |
| `GND`    | `GND` |
| `GPIO6`  | `SCK` |
| `GPIO7`  | `MOSI` |
| `GPIO23` | `CS` |
| `GPIO24` | `DC` |
| `GPIO27` | `BL` |
| `RST`    | `RST` |

### [GPS](https://a.co/d/hIqIitg)
| ESP32-C5 | GPS |
| -------- | --- |
| `3V3`    | `VCC` |
| `GND`    | `GND` |
| `GPIO13` | `RX` |
| `GPIO14` | `TX` |

### [SD Card](https://www.sparkfun.com/sparkfun-microsd-transflash-breakout.html)
| ESP32-C5 | SD |
| -------- | -- |
| `3V3`    | `VCC` |
| `GND`    | `GND` |
| `GPIO6`  | `SCK` |
| `GPIO2`  | `MISO` |
| `GPIO7`  | `MOSI` |
| `GPIO10` | `CS` |

### [Battery Fuel Gauge](https://www.adafruit.com/product/5580?srsltid=AfmBOorF18oKQ_UTmewFqWVfryc6hovloBa6APF5GGIUm1mz5bNJcq-2)
| Board | ESP32-C5 | Button |
| ----- | -------- | --- |
|       | `GPIO4` | `SCL` |
|       | `GPIO5` | `SDA` |
|       | `3V3` | `VCC` |
|       | `GND` | `GND` |
| `Battery +` | | `BAT` |

### Activity LED
| ESP32-C5 | LED |
| -------- | --- |
| `GPIO28` | `+` |
| `GND`    | `-` |

### User Buttons
The User Buttons require pull-down resistors
| ESP32-C5 | Button |
| -------- | --- |
| `GPIO8` | `DOWN` |
| `GPIO9` | `UP` |
| `GPIO15` | `SELECT` |

## Install Firmware
1. Clone this repo
2. In your workstation CLI, navigate to the `C5_Py_Flasher` directory
3. With your ESP32-C5 device unplugged, execute `python c5_flasher.py` and allow any missing python packages to install
4. Once you see `Waiting for ESP32-C5 device to be connected...`, connect your ESP32-C5 device to your PC via USB-C cable
5. Once you see `Ready to flash these files to ESP32-C5? (y/N):`, enter `y` and allow the firmware to flash
6. When the `Hardware reset` message appears on the screen, you may disconnect your ESP32-C5 device

## Update Firmware
The firmware update process is very simple once the initial install process is completed. The firmware is designed to check the attached SD card at every boot for a new bin file. If a new bin file is found, it uses it to automcatically execute an update. If an old bin file is found or no bin file is found, it resumes normal operation. 

1. Download latest firmware from releases
2. Place the .bin file on the root of your SD card
3. Install your SD card into the C5 wardriver
4. Boot the C5 wardriver and allow the automatic update process to execute

## Usage

### Booting
When the C5 Wardriver is powered on, it will attempt to connect to WiFi using the user-provided WiFi credentials. If there were not credentials provided or the provided WLAN is not available, the C5 Wardriver will then start its own access point with the name `c5wardriver` and the password `c5wardriver`. The access point will remain active for 60 seconds if no client connections are made. The access point will otherwise remain active until a client disconnected. If the C5 Wardriver is able to connect to WiFi, it will remain connected until 60 seconds of inactivity on it's web server has passed. Once the web server or access point inactivity threshold has been reached, the C5 Wardriver will start normal wardriving operation. You can skip the admin phase of the boot process and go straight to wardriving if you hold the `SELECT` button as soon as the JCMK logo appears on the display.

### Initial Setup
The first time you boot the C5 Wardriver, it will start its own access point named `c5wardriver`. You can connect to this access point using password `c5wardriver`. The access point will remain active for 60 seconds or until all connected clients disconnect. Once connected, you can navigate to `http://192.168.4.1` on your connected device where you will be presented with a web page. Here you can enter WiFi credentials which the C5 Wardriver will use to connect to a WLAN at every boot as well as your [Wigle API credentials](https://wigle.net/account). You can also find the full contents of your SD card root available for download, including your wardriving logs. Once WiFi credentials are saved, the access point and webserver shutdown and normal wardriving operation starts.

### Webserver Usage
At every boot, if the C5 Wardriver is able to connect to a WLAN using the user-provided WiFi credentials from the initial boot, it will present a webpage. The local IP of the C5 Wardriver will be displayed on the screen. The IP address may then be used by a device connected to the same WLAN to access the admin page of the C5 Wardriver. Here you will find the same web page as what you would see during the initial setup, the WLAN and Wigle credential fields, and SD card root contents. Here you can reconfigure your WLAN settings, download the contents of your SD card, and upload your wardriving logs directly to Wigle. Like the initial setup phase, there is a 60 second inactivity timeout. This means if you do not perform any actions on the web page for 60 seconds, the webserver will shutdown and normal wardriving operation will start. If the WLAN connection fails, the C5 Wardriver will start its own access point as described in [Initial Setup](#initial-setup).

### Buttons
During normal wardriving operation, the `UP` and `DOWN` buttons may be used to switch between display modes for different presentation of the wardriving status and statistic information.  
The initial WiFi admin sequence at boot can be skipped by holding the `SELECT` button as soon as the boot logo appears.
