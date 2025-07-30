#include "WiFiOps.h"

WebServer server(80);

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

      wifi_ops.setTotalBLECount(wifi_ops.getTotalBLECount() + 1);

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

void WiFiOps::setTotalNetCount(uint32_t count) {
  this->total_net_count = count;
}

void WiFiOps::setTotalBLECount(uint32_t count) {
  this->total_ble_count = count;
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

uint32_t WiFiOps::getTotalNetCount() {
  return this->total_net_count;
}

uint32_t WiFiOps::getTotalBLECount() {
  return this->total_ble_count;
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
      WiFi.scanNetworks(true, true, false, 80);
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

      this->setTotalNetCount(this->getTotalNetCount() + 1);

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

void WiFiOps::initWiFi(bool set_country) {
  if (set_country) {
    esp_wifi_init(&cfg);
    esp_wifi_set_country(&country);
  }

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

bool WiFiOps::tryConnectToWiFi(unsigned long timeoutMs) {

  display.clearScreen();
  display.tft->setCursor(0, 0);

  // Check if file exists
  if (!SPIFFS.exists(WIFI_CONFIG)) {
    Logger::log(WARN_MSG, "No saved WiFi config found.");
    return false;
  }

  display.tft->print("Joining WiFi: ");

  // Check if can open file
  File configFile = SPIFFS.open(WIFI_CONFIG, "r");
  if (!configFile) {
    Logger::log(WARN_MSG, "Failed to open config file.");
    display.tft->println("\nCould not get WiFi creds file");
    return false;
  }

  // Get json object ready
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  // Couldn't parse json from file
  if (error) {
    Logger::log(WARN_MSG, "Failed to parse config file.");
    display.tft->println("\nCould not parse WiFi creds");
    return false;
  }

  // Extract AP credentials
  const char* ssid = doc["ssid"];
  const char* password = doc["password"];
  this->user_ap_ssid = doc["ssid"].as<String>();
  this->user_ap_password = doc["password"].as<String>();
  this->wigle_user = doc["wigle_user"].as<String>();
  this->wigle_token = doc["wigle_token"].as<String>();

  Logger::log(STD_MSG, "Attempting to connect with: ");
  Logger::log(STD_MSG, ssid);
  display.tft->print(ssid);
  display.tft->println("...");

  // Connect to WiFi with AP credentials
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait while we connect
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  // Output status of connection attempt
  if (WiFi.status() == WL_CONNECTED) {
    display.clearScreen();
    display.tft->setCursor(0, 0);
    display.tft->print("Connected: ");
    display.tft->println(ssid);
    display.tft->print("IP: ");
    display.tft->println(WiFi.localIP());
    Logger::log(GUD_MSG, "WiFi connected!");
    Logger::log(GUD_MSG, "IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Logger::log(WARN_MSG, "Failed to connect to WiFi.");
    display.tft->println("Failed to connect");
    WiFi.disconnect(true);
    return false;
  }
}

void WiFiOps::startAccessPoint() {
  display.clearScreen();
  display.tft->setCursor(0, 0);
  display.tft->print("Starting AP: ");
  display.tft->println(this->apSSID);
  WiFi.softAP(this->apSSID, this->apPassword);
  Logger::log(GUD_MSG, "Access Point started");
  Logger::log(GUD_MSG, "IP: ");
  Logger::log(GUD_MSG, WiFi.softAPIP().toString());
  display.tft->print("IP: ");
  display.tft->println(WiFi.softAPIP().toString());
}

void WiFiOps::serveConfigPage() {
  server.on("/", HTTP_GET, [this]() {
    this->last_web_client_activity = millis();
    String html = R"rawliteral(
      <html><body>
      <h2>WiFi Configuration</h2>
      <form action="/save" method="POST">
        SSID: <input type="text" name="ssid"><br>
        Password: <input type="password" name="password"><br>
        WiGLE API Name: <input type="text" name="wigle_user"><br>
        WiGLE API Token: <input type="password" name="wigle_token"><br>
        <input type="submit" value="Save"><br><br>
      </form>
      <h2>Files on SD Card</h2>
    )rawliteral";

    File root = SD.open("/");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String filename = file.name();
          if ((filename.endsWith(".log")) && (this->connected_as_client)) {
            html += "<a href=\"/download?file=" + filename + "\">" + filename + "</a> | ";
            html += "<a href=\"/upload?file=" + filename + "\">Upload to WiGLE</a> " + (String)file.size() + " Bytes <br>";
          } else {
            html += "<a href=\"/download?file=" + filename + "\">" + filename + "</a> " + (String)file.size() + " Bytes <br>";
          }
        }
        file = root.openNextFile();
      }
    } else {
      html += "Unable to access SD card.<br>";
    }

    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/save", HTTP_POST, [this]() {
    this->last_web_client_activity = millis();
    if ((server.hasArg("ssid") && server.hasArg("password")) || (server.hasArg("wigle_user") && server.hasArg("wigle_token"))) {
      //String ssid = server.arg("ssid");
      //String password = server.arg("password");

      DynamicJsonDocument doc(256);
      //doc["ssid"] = ssid;
      //doc["password"] = password;

      if (server.hasArg("ssid")) {
        if (server.arg("ssid") != "") {
          doc["ssid"] = server.arg("ssid");
          this->user_ap_ssid = server.arg("ssid");
        } else {
          doc["ssid"] = this->user_ap_ssid;
        }
      } else {
        doc["ssid"] = this->user_ap_ssid;
      }
      if (server.hasArg("password")) {
        if (server.arg("password") != "") {
          doc["password"] = server.arg("password");
          this->user_ap_password = server.arg("password");
        } else {
          doc["password"] = this->user_ap_password;
        }
      } else {
        doc["password"] = this->user_ap_password;
      }
      if (server.hasArg("wigle_user")) {
        if (server.arg("wigle_user") != "") {
          doc["wigle_user"] = server.arg("wigle_user");
          this->wigle_user = server.arg("wigle_user");
        }
        else {
          doc["wigle_user"] = this->wigle_user;
        }
      } else {
        doc["wigle_user"] = this->wigle_user;
      }
      if (server.hasArg("wigle_token")) {
        if (server.arg("wigle_token") != "") {
          doc["wigle_token"] = server.arg("wigle_token");
          this->wigle_token = server.arg("wigle_token");
        } else {
          doc["wigle_token"] = this->wigle_token;
        }
      } else {
        doc["wigle_token"] = this->wigle_token;
      }

      Logger::log(STD_MSG, "SSID: " + this->user_ap_ssid);
      Logger::log(STD_MSG, "Wigle User: " + this->wigle_user);

      File configFile = SPIFFS.open(WIFI_CONFIG, FILE_WRITE);
      if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
        server.send(200, "text/html", "Credentials saved. You can close this window.");
        this->last_web_client_activity = 0;
        this->shutdownAccessPoint();
      } else {
        server.send(500, "text/plain", "Failed to save credentials.");
      }
    } else {
      server.send(400, "text/plain", "Missing SSID or password.");
    }
  });

  server.on("/download", HTTP_GET, [this]() {
    this->last_web_client_activity = millis();
    if (!server.hasArg("file")) {
      server.send(400, "text/plain", "Missing file parameter.");
      return;
    }

    String path = server.arg("file");
    Logger::log(GUD_MSG, "User downloading: " + path);
    if (!SD.exists("/" + path)) {
      server.send(404, "text/plain", "File not found.");
      return;
    }

    File downloadFile = SD.open("/" + path, FILE_READ);
    if (downloadFile) {
      server.sendHeader("Content-Disposition", "attachment; filename=\"" + path + "\"");
      server.streamFile(downloadFile, "application/octet-stream");
    }
    else
      Logger::log(GUD_MSG, "Failed to open file: " + path);
    downloadFile.close();
  });

  server.on("/upload", HTTP_GET, [this]() {
    if (!server.hasArg("file")) {
      server.send(400, "text/plain", "Missing file parameter.");
      this->last_web_client_activity = millis();
      return;
    }

    String filePath = "/" + server.arg("file");
    if (!SD.exists(filePath)) {
      server.send(404, "text/plain", "File not found.");
      this->last_web_client_activity = millis();
      return;
    }

    File fileToUpload = SD.open(filePath);
    if (!fileToUpload) {
      server.send(500, "text/plain", "Failed to open file.");
      this->last_web_client_activity = millis();
      return;
    }

    // Load credentials
    File configFile = SPIFFS.open(WIFI_CONFIG, "r");
    DynamicJsonDocument doc(512);
    deserializeJson(doc, configFile);
    configFile.close();

    String username = doc["wigle_user"] | "";
    String token = doc["wigle_token"] | "";
    if (username.isEmpty() || token.isEmpty()) {
      fileToUpload.close();
      server.send(500, "text/plain", "Missing WiGLE credentials.");
      this->last_web_client_activity = millis();
      return;
    }

    String boundary = "----ESP32BOUNDARY";
    String contentType = "multipart/form-data; boundary=" + boundary;

    // Build parts
    String part1 = "--" + boundary + "\r\n";
    part1 += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filePath + "\"\r\n";
    part1 += "Content-Type: application/octet-stream\r\n\r\n";

    String part2 = "\r\n--" + boundary + "\r\n";
    part2 += "Content-Disposition: form-data; name=\"donate\"\r\n\r\non\r\n";

    String part3 = "--" + boundary + "--\r\n";

    int totalLength = part1.length() + fileToUpload.size() + part2.length() + part3.length();

    // Connect manually via WiFiClientSecure
    WiFiClientSecure *client = new WiFiClientSecure();
    client->setInsecure();

    if (!client->connect("api.wigle.net", 443)) {
      fileToUpload.close();
      delete client;
      server.send(500, "text/plain", "Failed to connect to wigle.net.");
      this->last_web_client_activity = millis();
      return;
    }

    // Compose headers
    String auth = utils.base64Encode(username + ":" + token);
    client->println("POST /api/v2/file/upload HTTP/1.1");
    client->println("Host: api.wigle.net");
    client->println("User-Agent: ESP32Uploader/1.0");
    client->println("Accept: application/json");
    client->println("Authorization: Basic " + auth);
    client->println("Content-Type: " + contentType);
    client->print("Content-Length: ");
    client->println(totalLength);
    client->println();

    // Send body
    client->print(part1);
    const size_t BUFFER_SIZE = 4096; // 1KB at a time
    uint8_t buffer[BUFFER_SIZE];

    while (fileToUpload.available()) {
      size_t bytesRead = fileToUpload.read(buffer, BUFFER_SIZE);
      client->write(buffer, bytesRead);
    }
    client->print(part2);
    client->print(part3);

    fileToUpload.close();

    // Read response
    String response;
    unsigned long timeout = millis();
    while (client->connected() && millis() - timeout < 5000) {
      while (client->available()) {
        response += client->readStringUntil('\n');
      }
    }
    client->stop();
    delete client;

    Serial.println("WiGLE response:");
    Serial.println(response);
    server.send(200, "text/plain", "Upload complete. Response:\n" + response);
    this->last_web_client_activity = millis();
  });

  server.begin();
}

bool WiFiOps::monitorAP(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    this->showCountdown();
    delay(100);
    if (WiFi.softAPgetStationNum() > 0) {
      Logger::log(GUD_MSG, "Client connected.");
      return true;
    }
  }

  Logger::log(STD_MSG, "Timeout reached with no client.");
  shutdownAccessPoint();
  return false;
}

void WiFiOps::shutdownAccessPoint(bool ap_active) {
  if ((ap_active) && (WiFi.status() != WL_CONNECTED))
    Logger::log(STD_MSG, "Shutting down Access Point and Web Server...");
  else
    Logger::log(STD_MSG, "Shutting down Web Server...");

  server.stop();
  if (ap_active) {
    if (WiFi.status() != WL_CONNECTED)
      WiFi.softAPdisconnect(true);  // true = wipe SSID/password
  }
  delay(100);  // small delay for stability

  this->serving = false;
}

void WiFiOps::showCountdown() {
  if (millis() - this->last_timer > TIMER_UPDATE) {
    this->last_timer = millis();
    display.tft->fillRect(0, SMALL_CHAR_HEIGHT * 2, TFT_WIDTH, TFT_HEIGHT - (SMALL_CHAR_HEIGHT * 2), ST77XX_BLACK);
    display.tft->setCursor(0, SMALL_CHAR_HEIGHT * 4);
    display.tft->println("Wardring starts...\n");
    display.tft->setTextSize(2);
    display.tft->println(60 - ((millis() - this->last_web_client_activity) / 1000));
    display.tft->setTextSize(1);
  }
}

bool WiFiOps::begin(bool skip_admin) {
  this->current_scan_mode = WIFI_STANDBY;

  if (!skip_admin) {
    // Init WiFi
    this->initWiFi();

    // Run Admin stuff and wait for clients first
    bool connected = this->tryConnectToWiFi();

    this->connected_as_client = connected;

    if (!connected) {
      delay(1000);
      this->startAccessPoint();
    }

    this->serveConfigPage();

    this->serving = true;

    this->last_web_client_activity = millis();

    // Run AP loop
    if (!connected) {
      if (this->monitorAP()) {
        while (true) {
          server.handleClient();

          static bool wasConnected = WiFi.softAPgetStationNum() > 0;
          bool nowConnected = WiFi.softAPgetStationNum() > 0;

          if (wasConnected && !nowConnected) {
            Logger::log(STD_MSG, "Client disconnected.");
            this->shutdownAccessPoint();
            break;
          }

          wasConnected = nowConnected;
        }
      }
    } else { // Or run web server loop
      this->last_timer = millis();

      while (this->serving) {
        server.handleClient();

        this->showCountdown();

        if (millis() - this->last_web_client_activity > WEB_PAGE_TIMEOUT) {
          Logger::log(STD_MSG, "Web client activity timeout");
          Logger::log(STD_MSG, "Shutting down server and resuming normal function...");
          this->shutdownAccessPoint(false);
          break;
        }
      }
    }

    this->connected_as_client = false;

    this->deinitWiFi();
  }

  this->initWiFi(true);

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