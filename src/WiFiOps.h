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

#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "mbedtls/sha256.h"

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

typedef struct __attribute__((packed)) {
  char     magic[4];               // "ENOW"
  uint8_t  type;                   // MSG_TEXT
  uint32_t counter;                // heartbeat counter (valid for MSG_HEARTBEAT)
  uint16_t len;                    // number of bytes in text (not including NUL)
  char     text[ENOW_TEXT_MAX + 1];  // +1 for NUL terminator
} enow_text_msg_t;

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
    #ifdef CORE
      int run_mode = CORE_MODE;
    #elif defined(NODE)
      int run_mode = NODE_MODE;
    #else
      int run_mode = SOLO_MODE;
    #endif

    uint mac_history_cursor = 0;
    bool clientConnected = false;
    bool serving = false;
    uint32_t last_web_client_activity;
    uint32_t last_timer;
    bool use_encryption = false;

    String esp_now_key = "";

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
    void startESPNow();
    bool getHasCore();
    bool getSecureReady();
    bool getNodeReady();
    bool sendEncryptedStringToCore(const String& s);
    bool sendBroadcastStringPlain(const String& s);

    void startAccessPoint();
    void serveConfigPage();
    bool monitorAP(unsigned long timeoutMs = WEB_PAGE_TIMEOUT);

    static void setFixedChannel(uint8_t ch);
    static bool addPeerWithMode(const uint8_t* mac, bool encrypt, const uint8_t lmk16[16]);
    static void sendCoreRequest();
    static void sendCoreReply(const uint8_t* destMac);
    static void sendHeartbeat();
    static void OnDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    static void derive_key_16(const String& s, uint8_t out16[16]);
    static void computeKeysFromEnowKey();

};

#endif