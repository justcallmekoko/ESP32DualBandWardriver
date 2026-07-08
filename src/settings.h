#pragma once

#ifndef Settings_h
#define Settings_h

#include "configs.h"
#include "logger.h"
#include "utils.h"

#include "SPIFFS.h"
#include <FS.h>
#include <ArduinoJson.h>

#define FORMAT_SPIFFS_IF_FAILED true

#define CLICK_NAME "Click"
#define ALARM_NAME "Alarm"
#define VIBE_NAME  "Vibration"
#define TIME_NAME  "Timestamp"
#define SEN_SUB    "Subghz Sens"
#define SEN_2G4    "2.4ghz Sens"
#define SEN_5G8    "5.8ghz Sens"
#define LOW_SENS   1
#define MED_SENS   2
#define HIGH_SENS  3

// ============================================================
// Chunk 1: New setting key names
// ============================================================
#define WDG_KEY_NAME       "wdg_key"   // WDG Wars API key (String)
#define TRIGGER_SSID_NAME  "t_ssid"    // Dock trigger SSID e.g. "K1T" (String)
#define TRIGGER_PASS_NAME  "t_pass"    // Dock trigger SSID password (String)
#define ADMIN_PASS_NAME    "a_pass"    // Basic Auth password for web UI (String)
#define DEBUG_LOG_NAME     "dbg_en"    // SD debug log enabled (bool)
// SSID exclusion list keys:  "sx_0" through "sx_9"   (String each)
// Geofence keys:             "geo_0" through "geo_4" (String, JSON encoded)
// Geofence JSON format: {"lat":0.000000,"lon":0.000000,"rad":0,"label":""}

class Settings {

  private:
    String json_settings_string;

  public:
    bool begin();

    template <typename T>
    T loadSetting(String name);

    template <typename T>
    T loadSettingMin(String name);

    template <typename T>
    T loadSettingMax(String name);

    template <typename T>
    T saveSetting(String key, bool value);

    template <typename T>
    T saveSetting(String key, int value, bool is_int = true);

    template <typename T>
    T saveSetting(String key, String value);

    bool toggleSetting(String key);
    String getSettingType(String key);
    String setting_index_to_name(int i);
    int getNumberSettings();

    String getSettingsString();
    bool createDefaultSettings(fs::FS &fs, bool spec = false, uint8_t index = 0, String typeStr = "bool", String name = "");
    void printJsonSettings(String json_string);
    void wipeSPIFFS();
    void main(uint32_t currentTime);
};

#endif
