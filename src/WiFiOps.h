#pragma once
#ifndef WiFiOps_h
#define WiFiOps_h

#include "configs.h"
#include "utils.h"
#include "settings.h"
#include "GpsInterface.h"
#include "Buffer.h"
#include "display.h"
#include "SDInterface.h"

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

#include <NimBLEDevice.h> // 2.3.0

extern GpsInterface gps;
extern SDInterface sd_obj;
extern Buffer buffer;
extern Utils utils;
extern Settings settings;
extern Display display;

extern WebServer server;

#define WIFI_STANDBY    0
#define WIFI_WARDRIVING 1
#define WIFI_UPDATE     2

class WiFiOps
{
  private:
    NimBLEScan* pBLEScan;

    wifi_country_t country = {
      .cc = "PH",
      .schan = 1,
      .nchan = 13,
      .policy = WIFI_COUNTRY_POLICY_AUTO,
    };

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    const char* apSSID = "c5wardriver";
    const char* apPassword = "c5wardriver";

    WiFiClientSecure *client = new WiFiClientSecure();

    String user_ap_ssid = "";
    String user_ap_password = "";
    String wigle_user = "";
    String wigle_token = "";

    bool connected_as_client = false;

    uint8_t current_scan_mode;
    uint32_t init_time;
    struct mac_addr mac_history[mac_history_len];

    uint32_t current_net_count = 0;
    uint32_t current_2g4_count = 0;
    uint32_t current_5g_count = 0;
    uint32_t current_ble_count = 0;
    uint32_t total_net_count = 0;
    uint32_t total_ble_count = 0;

    void showCountdown();
    int runWardrive(uint32_t currentTime);
    void scanBLE();
    bool mac_cmp(struct mac_addr addr1, struct mac_addr addr2);
    void clearMacHistory();
    String security_int_to_string(int security_type);
    void processWardrive(uint16_t networks);
    void shutdownAccessPoint(bool ap_active = true);

  public:
    uint mac_history_cursor = 0;
    bool clientConnected = false;
    bool serving = false;
    uint32_t last_web_client_activity;
    uint32_t last_timer;

    bool begin(bool skip_admin = false);
    void main(uint32_t currentTime);
    void startLog(String file_name);
    void initBLE();
    void initWiFi(bool set_country = false);
    void deinitBLE();
    void deinitWiFi();
    bool tryConnectToWiFi(unsigned long timeoutMs = STATION_CONNECT_TIMEOUT);
    bool backendUpload(String filePath);
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

    void startAccessPoint();
    void serveConfigPage();
    bool monitorAP(unsigned long timeoutMs = WEB_PAGE_TIMEOUT);

};

#endif