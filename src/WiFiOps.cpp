#include "WiFiOps.h"

// Add compiler.c.elf.extra_flags=-Wl,-zmuldefs to platform.txt
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3){
  if (arg == 31337)
    return 1;
  else
    return 0;
}

class scanCallbacks : public NimBLEScanCallbacks {

  void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {

    extern WiFiOps wifi_ops;

    uint8_t macBytes[6];

    if ((gps.getGpsModuleStatus()) && (gps.getFixStatus()) && (sd_obj.supported)) {
      
      utils.stringToMac(advertisedDevice->getAddress().toString().c_str(), macBytes);

      if (wifi_ops.seen_mac(macBytes))
        return;

      wifi_ops.save_mac(macBytes);

      wifi_ops.setCurrentBLECount(wifi_ops.getCurrentBLECount() + 1);

      bool do_save = false;

      if (gps.getFixStatus())
        do_save = true;

      String wardrive_line = (String)advertisedDevice->getAddress().toString().c_str() + ",,[BLE]," + gps.getDatetime() + ",0," + (String)advertisedDevice->getRSSI() + "," + gps.getLat() + "," + gps.getLon() + "," + gps.getAlt() + "," + gps.getAccuracy() + ",BLE";
      Logger::log(GUD_MSG, (String)wifi_ops.mac_history_cursor + " | " + wardrive_line);

      if (do_save)
        buffer.append(wardrive_line + "\n");
    }
  }
};

void WiFiOps::setCurrentScanMode(uint8_t scan_mode) {
  this->current_scan_mode = scan_mode;
}

uint8_t WiFiOps::getCurrentScanMode() {
  return this->current_scan_mode;
}

void WiFiOps::setCurrentNetCount(uint32_t count) {
  this->current_net_count = count;
}

void WiFiOps::setCurrent2g4Count(uint32_t count) {
  this->current_2g4_count = count;
}

void WiFiOps::setCurrent5gCount(uint32_t count) {
  this->current_5g_count = count;
}

void WiFiOps::setCurrentBLECount(uint32_t count) {
  this->current_ble_count = count;
}

uint32_t WiFiOps::getCurrentNetCount() {
  return this->current_net_count;
}

uint32_t WiFiOps::getCurrent2g4Count() {
  return this->current_2g4_count;
}

uint32_t WiFiOps::getCurrent5gCount() {
  return this->current_5g_count;
}

uint32_t WiFiOps::getCurrentBLECount() {
  return this->current_ble_count;
}

void WiFiOps::scanBLE() {
  //Logger::log(STD_MSG, "Starting BLE scan...");
  pBLEScan->start(BLE_SCAN_DURATION, false, false);
  //Logger::log(STD_MSG, "Completed BLE scan");
}

int WiFiOps::runWardrive(uint32_t currentTime) {

  int scan_status = -1;

  // Check GPS status
  if ((gps.getGpsModuleStatus()) && (gps.getFixStatus()) && (sd_obj.supported)) {

    scan_status = WiFi.scanComplete();

    // Pause if scan is running already
    if (scan_status == WIFI_SCAN_RUNNING) // Scan is still running
      delay(1);
    else if (scan_status == WIFI_SCAN_FAILED) { // Scan is failed or not started
      WiFi.scanNetworks(true, true, false, 110);
      delay(100);
      if (WiFi.scanComplete() == WIFI_SCAN_FAILED)
        Logger::log(WARN_MSG, "WiFi scan failed to start!");
    }
    else {
      this->current_net_count = 0;
      this->current_ble_count = 0;
      this->current_2g4_count = 0;
      this->current_5g_count = 0;

      // Scan has completed and is number of networks found
      // Handle the scan results
      this->processWardrive(scan_status);

      // Delete the scan data
      WiFi.scanDelete();

      // Scan BLE here
      this->scanBLE();

      while(pBLEScan->isScanning())
        delay(1);

      // Start a new scan on all channels
      WiFi.scanNetworks(true, true, false, 110);
    }
  }

  return scan_status;
}

void WiFiOps::processWardrive(uint16_t networks) {
  String display_string;
  bool do_save;

  // Process results if networks found
  if (networks > 0) {
    for (int i = 0; i < networks; i++) {
      digitalWrite(LED_PIN, HIGH);
      display_string = "";
      do_save = false;
      uint8_t *this_bssid_raw = WiFi.BSSID(i);
      char this_bssid[18] = {0};
      sprintf(this_bssid, "%02X:%02X:%02X:%02X:%02X:%02X", this_bssid_raw[0], this_bssid_raw[1], this_bssid_raw[2], this_bssid_raw[3], this_bssid_raw[4], this_bssid_raw[5]);

      if (this->seen_mac(this_bssid_raw))
        continue;

      this->save_mac(this_bssid_raw);

      this->setCurrentNetCount(this->getCurrentNetCount() + 1);

      if (WiFi.channel(i) > 14)
        this->setCurrent5gCount(this->getCurrent5gCount() + 1);
      else
        this->setCurrent2g4Count(this->getCurrent2g4Count() + 1);

      String ssid = WiFi.SSID(i);
      ssid.replace(",","_");

      if (ssid != "") {
        display_string.concat(ssid);
      }
      else {
        display_string.concat(this_bssid);
      }

      if (gps.getFixStatus()) {
        do_save = true;
        display_string.concat(" | Lt: " + gps.getLat());
        display_string.concat(" | Ln: " + gps.getLon());
      }
      else {
        display_string.concat(" | GPS: No Fix");
      }

      int temp_len = display_string.length();

      #ifdef HAS_SCREEN
        for (int i = 0; i < 40 - temp_len; i++)
        {
          display_string.concat(" ");
        }
        
        //display_obj.display_buffer->add(display_string);
      #endif


      String wardrive_line = WiFi.BSSIDstr(i) + "," + ssid + "," + this->security_int_to_string(WiFi.encryptionType(i)) + "," + gps.getDatetime() + "," + (String)WiFi.channel(i) + "," + (String)WiFi.RSSI(i) + "," + gps.getLat() + "," + gps.getLon() + "," + gps.getAlt() + "," + gps.getAccuracy() + ",WIFI";
      Logger::log(GUD_MSG, (String)this->mac_history_cursor + " | " + wardrive_line);

      digitalWrite(LED_PIN, LOW);

      if (do_save) {
        buffer.append(wardrive_line + "\n");
      }
    }
  }

  digitalWrite(LED_PIN, LOW);
}

bool WiFiOps::mac_cmp(struct mac_addr addr1, struct mac_addr addr2) {
  //Return true if 2 mac_addr structs are equal.
  for (int y = 0; y < 6 ; y++) {
    if (addr1.bytes[y] != addr2.bytes[y]) {
      return false;
    }
  }
  return true;
}

bool WiFiOps::seen_mac(unsigned char* mac) {
  //Return true if this MAC address is in the recently seen array.

  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = mac[x];
  }

  for (int x = 0; x < mac_history_len; x++) {
    if (this->mac_cmp(tmp, mac_history[x])) {
      return true;
    }
  }
  return false;
}

void WiFiOps::save_mac(unsigned char* mac) {
  //Save a MAC address into the recently seen array.
  if (this->mac_history_cursor >= mac_history_len) {
    this->mac_history_cursor = 0;
  }
  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = mac[x];
  }

  mac_history[this->mac_history_cursor] = tmp;
  this->mac_history_cursor++;
}

String WiFiOps::security_int_to_string(int security_type) {
  //Provide a security type int from WiFi.encryptionType(i) to convert it to a String which Wigle CSV expects.
  String authtype = "";

  switch (security_type) {
    case WIFI_AUTH_OPEN:
      authtype = "[OPEN]";
      break;
  
    case WIFI_AUTH_WEP:
      authtype = "[WEP]";
      break;
  
    case WIFI_AUTH_WPA_PSK:
      authtype = "[WPA_PSK]";
      break;
  
    case WIFI_AUTH_WPA2_PSK:
      authtype = "[WPA2_PSK]";
      break;
  
    case WIFI_AUTH_WPA_WPA2_PSK:
      authtype = "[WPA_WPA2_PSK]";
      break;
  
    case WIFI_AUTH_WPA2_ENTERPRISE:
      authtype = "[WPA2]";
      break;

    //Requires at least v2.0.0 of https://github.com/espressif/arduino-esp32/
    case WIFI_AUTH_WPA3_PSK:
      authtype = "[WPA3_PSK]";
      break;

    case WIFI_AUTH_WPA2_WPA3_PSK:
      authtype = "[WPA2_WPA3_PSK]";
      break;

    case WIFI_AUTH_WAPI_PSK:
      authtype = "[WAPI_PSK]";
      break;
        
    default:
      authtype = "[UNDEFINED]";
  }

  return authtype;
}

void WiFiOps::clearMacHistory() {
    for (int i = 0; i < mac_history_len; ++i) {
        memset(mac_history[i].bytes, 0, sizeof(mac_history[i].bytes));
    }
}

void WiFiOps::startLog(String file_name) {
  buffer.logOpen(
    file_name,
    #if defined(HAS_SD)
      sd_obj.supported ? &SD :
    #endif
    NULL,
    false // Set with commandline options
  );
}

void WiFiOps::initWiFi() {
  WiFi.STA.begin();
  WiFi.setBandMode(WIFI_BAND_MODE_AUTO);
  delay(100);
}

void WiFiOps::deinitWiFi() {
  WiFi.disconnect(true);
}

void WiFiOps::deinitBLE() {
  pBLEScan->stop();
  pBLEScan->clearResults();
  NimBLEDevice::deinit();
}

void WiFiOps::initBLE() {
  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan();

  pBLEScan->setScanCallbacks(new scanCallbacks(), false);
  pBLEScan->setActiveScan(true);
  pBLEScan->setDuplicateFilter(false);       // Disables internal filtering based on MAC
  pBLEScan->setMaxResults(0);                // Prevent storing results in NimBLEScanResults
}

bool WiFiOps::begin() {
  this->current_scan_mode = WIFI_STANDBY;

  // Init WiFi
  this->initWiFi();

  // Init NimBLE
  this->initBLE();

  startLog("wardrive");
  String header_line = "WigleWifi-1.4,appRelease=" + (String)FIRMWARE_VERSION + ",model=" + (String)DEVICE_NAME + ",release=" + (String)FIRMWARE_VERSION + ",device=" + (String)DEVICE_NAME + ",display=SPI TFT,board=ESP32-C5-DevKit,brand=JustCallMeKoko\nMAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type";
  buffer.append(header_line + "\n");
  Logger::log(GUD_MSG, "Wigle Header: " + header_line);

  this->init_time = millis();

  return true;
}

void WiFiOps::main(uint32_t currentTime) {
  if (this->current_scan_mode == WIFI_WARDRIVING)
    this->runWardrive(currentTime);
}