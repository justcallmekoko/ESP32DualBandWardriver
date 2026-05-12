#include "BatteryInterface.h"

BatteryInterface::BatteryInterface() {
}

void BatteryInterface::main(uint32_t currentTime) {
  if (currentTime != 0) {
    if (currentTime - initTime >= 3000) {
      this->initTime = millis();

      // Battery level update
      int8_t new_level = this->getBatteryLevel();
      if (this->battery_level != new_level) {
        Logger::log(STD_MSG, "Battery Level changed: " + (String)new_level);
        this->battery_level = new_level;
        Logger::log(STD_MSG, "Battery Level: " + (String)this->battery_level);
      }
    }

    // Chunk 2: sample chargeRate on its own slower interval
    if (this->has_max17048 &&
        currentTime - this->last_rate_sample >= CHARGE_RATE_SAMPLE_MS) {
      this->last_rate_sample = currentTime;

      float rate = this->maxlipo.chargeRate();
      Logger::log(STD_MSG, "[CHG] chargeRate: " + String(rate, 2) +
                  " %/hr | state: " +
                  (this->charging_state ? "POWER" : "BATTERY"));

      if (rate < CHARGE_RATE_THRESHOLD) {
        // Rate is negative enough to suggest battery-only
        this->low_rate_count++;
        Logger::log(STD_MSG, "[CHG] Low rate count: " +
                    String(this->low_rate_count) + "/" +
                    String(CHARGE_RATE_CONFIRM));

        if (this->low_rate_count >= CHARGE_RATE_CONFIRM &&
            this->charging_state) {
          this->charging_state = false;
          Logger::log(WARN_MSG, "[CHG] Power removed — switching to BATTERY");
        }
      } else {
        // Rate is non-negative — power is present
        if (this->low_rate_count > 0) {
          Logger::log(STD_MSG, "[CHG] Rate recovered — resetting low count");
          this->low_rate_count = 0;
        }
        if (!this->charging_state) {
          this->charging_state = true;
          Logger::log(GUD_MSG, "[CHG] Power restored — switching to POWER");
        }
      }
    }
  }
}

// ============================================================
// Chunk 2: Public accessor — returns debounced power state.
// true  = power present (charging or charge-complete)
// false = battery only
// ============================================================
bool BatteryInterface::isCharging() {
  return this->charging_state;
}

void BatteryInterface::RunSetup() {
  byte error;

  #ifdef HAS_BATTERY

    Wire.begin(I2C_SDA, I2C_SCL);

    Logger::log(STD_MSG, "Checking for battery monitors...");

    Wire.beginTransmission(IP5306_ADDR);
    error = Wire.endTransmission();

    if (error == 0) {
      Logger::log(GUD_MSG, "Detected IP5306");
      this->has_ip5306    = true;
      this->i2c_supported = true;
    }

    Wire.beginTransmission(MAX17048_ADDR);
    error = Wire.endTransmission();

    if (error == 0) {
      if (maxlipo.begin()) {
        Logger::log(GUD_MSG, "Detected MAX17048");
        this->has_max17048  = true;
        this->i2c_supported = true;
      }
    }

    this->initTime        = millis();
    this->last_rate_sample = millis();

    // Chunk 2: seed initial state — assume power present at boot.
    // The first real sample fires after CHARGE_RATE_SAMPLE_MS (45s).
    // Conservative default avoids a false power-off trigger at startup.
    this->charging_state = true;
    this->low_rate_count = 0;
    Logger::log(STD_MSG, "[CHG] Initial state: POWER (assumed at boot)");

  #endif
}

int8_t BatteryInterface::getBatteryLevel() {

  if (this->has_ip5306) {
    Wire.beginTransmission(IP5306_ADDR);
    Wire.write(0x78);
    if (Wire.endTransmission(false) == 0 &&
        Wire.requestFrom(IP5306_ADDR, 1)) {
      this->i2c_supported = true;
      switch (Wire.read() & 0xF0) {
        case 0xE0: return 25;
        case 0xC0: return 50;
        case 0x80: return 75;
        case 0x00: return 100;
        default:   return 0;
      }
    }
    this->i2c_supported = false;
    return -1;
  }

  if (this->has_max17048) {
    float percent = this->maxlipo.cellPercent();

    if (percent >= 100)
      return 100;
    else if (percent <= 0)
      return 0;
    else
      return (int8_t)percent;
  }

  return 0;
}
