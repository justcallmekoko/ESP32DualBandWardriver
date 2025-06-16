#pragma once
#ifndef WiFiOps_h
#define WiFiOps_h

#include "configs.h"
#include "utils.h"
#include "settings.h"
#include "GpsInterface.h"
#include "Buffer.h"
#include "SDInterface.h"

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

#include <NimBLEDevice.h>

extern GpsInterface gps;
extern SDInterface sd_obj;
extern Buffer buffer;
extern Utils utils;
extern Settings settings;

#define WIFI_STANDBY    0
#define WIFI_WARDRIVING 1
#define WIFI_UPDATE     2

class WiFiOps
{
  private:
    NimBLEScan* pBLEScan;

    uint8_t current_scan_mode;
    uint32_t init_time;
    struct mac_addr mac_history[mac_history_len];

    uint32_t current_net_count = 0;
    uint32_t current_2g4_count = 0;
    uint32_t current_5g_count = 0;
    uint32_t current_ble_count = 0;
    uint32_t total_net_count = 0;
    uint32_t total_ble_count = 0;

    void initWiFi();
    void initBLE();
    void deinitWiFi();
    void deinitBLE();
    int runWardrive(uint32_t currentTime);
    void scanBLE();
    bool mac_cmp(struct mac_addr addr1, struct mac_addr addr2);
    void clearMacHistory();
    String security_int_to_string(int security_type);
    void processWardrive(uint16_t networks);
    void startLog(String file_name);

  public:
    uint mac_history_cursor = 0;

    bool begin();
    void main(uint32_t currentTime);
    void setCurrentScanMode(uint8_t scan_mode);
    uint8_t getCurrentScanMode();
    void setTotalNetCount(uint32_t count);
    void setTotalBLECount(uint32_t count);
    void setCurrentNetCount(uint32_t count);
    void setCurrent2g4Count(uint32_t count);
    void setCurrent5gCount(uint32_t count);
    void setCurrentBLECount(uint32_t count);
    uint32_t getTotalNetCount();
    uint32_t getTotalBLECount();
    uint32_t getCurrentNetCount();
    uint32_t getCurrent2g4Count();
    uint32_t getCurrent5gCount();
    uint32_t getCurrentBLECount();
    bool seen_mac(unsigned char* mac);
    void save_mac(unsigned char* mac);

};

#endif