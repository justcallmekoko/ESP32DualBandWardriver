#pragma once

#ifndef BatteryInterface_h
#define BatteryInterface_h

#include <Arduino.h>

#include "configs.h"
#include "Adafruit_MAX1704X.h"
#include "utils.h"
#include "logger.h"

#include <Wire.h>

#define IP5306_ADDR   0x75
#define MAX17048_ADDR 0x36

// ============================================================
// Chunk 2: Power presence detection via MAX17048 chargeRate()
//
// The XB8608 charger IC has no I2C — hardware CHG signal on
// GPIO25 only reflects UART charging path, not USB power.
// Instead we use MAX17048 chargeRate() (%/hr, CRATE register,
// updates every ~45s):
//   positive  = gaining charge  → USB/UART power present
//   near zero = float/full      → power present (conservative)
//   < -1.0    = discharging     → battery only
//
// Two consecutive sub-threshold readings required before
// declaring battery-only to filter transient noise.
// Detection latency: up to ~90s (2 x 45s CRATE update cycle).
// This is acceptable for a 5-minute power-off timer.
// ============================================================
#define CHARGE_RATE_THRESHOLD   -1.0f  // %/hr below this = battery only
#define CHARGE_RATE_SAMPLE_MS   45000  // sample every 45s (CRATE update cycle)
#define CHARGE_RATE_CONFIRM     2      // consecutive low readings to confirm

class BatteryInterface {
  private:
    uint32_t initTime          = 0;
    Adafruit_MAX17048 maxlipo;

    // Chunk 2: chargeRate-based power detection
    bool     charging_state    = true;  // assume power present at boot
    uint8_t  low_rate_count    = 0;     // consecutive sub-threshold readings
    uint32_t last_rate_sample  = 0;     // millis() of last chargeRate sample

  public:
    int8_t battery_level = 0;
    int8_t old_level     = 0;
    bool   i2c_supported = false;
    bool   has_max17048  = false;
    bool   has_ip5306    = false;

    BatteryInterface();

    void   RunSetup();
    void   main(uint32_t currentTime);
    int8_t getBatteryLevel();

    // Chunk 2: returns current power-present state.
    // true  = USB/UART power present (charging or charge-complete)
    // false = running on battery only
    bool isCharging();
};

#endif
