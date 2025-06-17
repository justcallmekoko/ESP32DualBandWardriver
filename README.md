# ESP32 Dual Band Wardriver
Based on the ESP32-C5-DevKitC-1 v1.2, the ESP32 Dual Band Wardriver offers wardriving capabilities for 2.4GHz and 5GHz WiFi as well as BLE.
Logs are formatted for Wigle and saved to SD card.

## Connections

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

## Programming
*to-do*
