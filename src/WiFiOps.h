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
    uint8_t current_scan_mode;
    uint32_t init_time;
    uint mac_history_cursor = 0;
    struct mac_addr mac_history[mac_history_len];

    int runWardrive(uint32_t currentTime);
    bool mac_cmp(struct mac_addr addr1, struct mac_addr addr2);
    bool seen_mac(unsigned char* mac);
    void save_mac(unsigned char* mac);
    void clearMacHistory();
    String security_int_to_string(int security_type);
    void processWardrive(uint16_t networks);
    void startLog(String file_name);

  public:

    bool begin();
    void main(uint32_t currentTime);
    void setCurrentScanMode(uint8_t scan_mode);
    uint8_t getCurrentScanMode();

};

#endif