#include "ui.h"

void UI::begin() {
  sd_file_menu.list = new LinkedList<MenuNode>();
  action_menu.list  = new LinkedList<MenuNode>();
  mode_menu.list    = new LinkedList<MenuNode>();
  upload_menu.list  = new LinkedList<MenuNode>();
  delete_all_menu.list = new LinkedList<MenuNode>();
  upload_all_menu.list = new LinkedList<MenuNode>();
  mark_geofence_menu.list = new LinkedList<MenuNode>();

  mode_menu.name   = "Mode";
  action_menu.name = "Action";
  upload_menu.name = "Upload";
  delete_all_menu.name = "Delete All?";
  upload_all_menu.name = "Upload All?";
  mark_geofence_menu.name = "Mark Geofence Center?";

  this->buildSDFileMenu();

  action_menu.parentMenu = &sd_file_menu;
  mode_menu.parentMenu   = &sd_file_menu;
  upload_menu.parentMenu = &action_menu;  // Upload is a submenu of Action
  delete_all_menu.parentMenu = &sd_file_menu;
  upload_all_menu.parentMenu = &sd_file_menu;
  mark_geofence_menu.parentMenu = &sd_file_menu;

  // Geofence menu
  this->addNodes(&mark_geofence_menu, "No", ST77XX_WHITE, NULL, 0, [this]() {
    this->current_menu = mark_geofence_menu.parentMenu;
  });
  this->addNodes(&mark_geofence_menu, "Yes", ST77XX_WHITE, NULL, 0, [this]() {
    display.clearScreen();

    // Check to make sure we have GPS
    if (gps.getFixStatus() && gps.getGpsModuleStatus()) {
      bool save_available = false;

      // Look for next available geofence
      for (int i = 0; i < MAX_GEOFENCES; i++) {
        String geoStr = settings.loadSetting<String>("geo_" + String(i));

        // Parse stored JSON geo string
        DynamicJsonDocument geoDoc(256);
        if (!geoStr.isEmpty() && deserializeJson(geoDoc, geoStr) == DeserializationError::Ok) {
          float gLat = geoDoc["lat"];
          float gLon = geoDoc["lon"];
          int gRad = geoDoc["rad"];
          String old_label = geoDoc["label"];
          if (gLat != 0.0 && gLon != 0.0 && old_label != "") {
            Logger::log(STD_MSG, "geo_" + String(i) + " lat: " + String(gLat) + ", label: " + old_label + " exists. Skipping...");
            continue;
          }
          else {
            save_available = true;
            Logger::log(STD_MSG, "geo_" + String(i) + " available. Creating...");
            int rad = (int)(0.10 * 1609.34);     // convert to meters for storage
            String label = "Live Geofence " + String(i);

            DynamicJsonDocument geoDoc(256);
            geoDoc["lat"]   = gps.getLat().toFloat();
            geoDoc["lon"]   = gps.getLon().toFloat();
            geoDoc["rad"]   = rad;
            geoDoc["label"] = label;
            String geoStr;
            serializeJson(geoDoc, geoStr);

            settings.saveSetting<bool>("geo_" + String(i), geoStr);

            wifi_ops.reloadGeofenceCache();

            Logger::log(GUD_MSG, "Saved geofence center \"" + label + "\" as geo_" + String(i));

            display.drawCenteredText("New Geofence Saved", true);

            break;

          }
        }
      }
      if (!save_available) {
        display.drawCenteredText("No available save slots", true);
      }
    }
    else {
      display.drawCenteredText("Need GPS Fix", true);
    }

    delay(2000);

    this->current_menu = &sd_file_menu;
  });

  // Delete all Menu
  this->addNodes(&delete_all_menu, "No", ST77XX_WHITE, NULL, 0, [this]() {
    this->current_menu = delete_all_menu.parentMenu;
  });
  this->addNodes(&delete_all_menu, "Yes", ST77XX_WHITE, NULL, 0, [this]() {
    display.clearScreen();

    display.drawCenteredText("Deleting Logs...");

    buffer.setFileName("");

    for (int i = 0; i < sd_obj.sd_files->size(); i++) {
      if (sd_obj.sd_files->get(i).startsWith("wardrive_") || sd_obj.sd_files->get(i).startsWith("wigle-")) {
        if (sd_obj.removeFile("/" + sd_obj.sd_files->get(i))) {
          Logger::log(STD_MSG, "Removed file: " + sd_obj.sd_files->get(i));
          sd_obj.removeFile("/" + sd_obj.sd_files->get(i) + ".wdg");
          sd_obj.removeFile("/" + sd_obj.sd_files->get(i) + ".wigle");
        }
        else {
          Logger::log(WARN_MSG, "Could not remove file: " + sd_obj.sd_files->get(i));
        }
      }
    }
    display.clearScreen();

    display.drawCenteredText("Logs removed");

    delay(2000);

    this->buildSDFileMenu();

    this->current_menu = &sd_file_menu;
  });

  // Upload all Menu
  this->addNodes(&upload_all_menu, "Back", ST77XX_WHITE, NULL, 0, [this]() {
    this->current_menu = upload_all_menu.parentMenu;
  });
  this->addNodes(&upload_all_menu, "WiGLE", ST77XX_WHITE, NULL, 0, [this]() {
    if (wifi_ops.tryConnectToWiFi()) {
      delay(1000);
      for (int i = 0; i < sd_obj.sd_files->size(); i++) {
        if (sd_obj.sd_files->get(i).startsWith("wardrive_") || sd_obj.sd_files->get(i).startsWith("wigle-")) {
          Logger::log(STD_MSG, "Uploading " + sd_obj.sd_files->get(i) + "...");
          if (wifi_ops.uploadFile("/" + sd_obj.sd_files->get(i), true, WIGLE_UPLOAD)) {
            display.clearScreen();
            display.drawCenteredText("WiGLE OK", true);
          } else {
            display.clearScreen();
            display.drawCenteredText("WiGLE failed", true);
          }
        }
      }
    }
    wifi_ops.deinitWiFi();
    delay(10);
    wifi_ops.initWiFi();
    delay(2000);
    this->current_menu = upload_all_menu.parentMenu;
  });
  this->addNodes(&upload_all_menu, "WDGWars", ST77XX_WHITE, NULL, 0, [this]() {
    if (wifi_ops.tryConnectToWiFi()) {
      delay(1000);
      for (int i = 0; i < sd_obj.sd_files->size(); i++) {
        if (sd_obj.sd_files->get(i).startsWith("wardrive_") || sd_obj.sd_files->get(i).startsWith("wigle-")) {
          Logger::log(STD_MSG, "Uploading " + sd_obj.sd_files->get(i) + "...");
          if (wifi_ops.uploadFile("/" + sd_obj.sd_files->get(i), true, WDG_UPLOAD)) {
            display.clearScreen();
            display.drawCenteredText("WDG OK", true);
          } else {
            display.clearScreen();
            display.drawCenteredText("WDG failed", true);
          }
        }
      }
    }
    wifi_ops.deinitWiFi();
    delay(10);
    wifi_ops.initWiFi();
    delay(2000);
    this->current_menu = upload_all_menu.parentMenu;
  });
  this->addNodes(&upload_all_menu, "Both", ST77XX_WHITE, NULL, 0, [this]() {
    if (wifi_ops.tryConnectToWiFi()) {
      delay(1000);
      for (int i = 0; i < sd_obj.sd_files->size(); i++) {
        if (sd_obj.sd_files->get(i).startsWith("wardrive_") || sd_obj.sd_files->get(i).startsWith("wigle-")) {
          Logger::log(STD_MSG, "Uploading " + sd_obj.sd_files->get(i) + "...");
          if (wifi_ops.uploadFile("/" + sd_obj.sd_files->get(i), true, BOTH_UPLOAD)) {
            display.clearScreen();
            display.drawCenteredText("Upload OK", true);
          } else {
            display.clearScreen();
            display.drawCenteredText("Upload failed", true);
          }
        }
      }
    }
    wifi_ops.deinitWiFi();
    delay(10);
    wifi_ops.initWiFi();
    delay(2000);
    this->current_menu = upload_all_menu.parentMenu;
  });

  this->addNodes(&action_menu, "Back", ST77XX_WHITE, NULL, 0, [this]() {
    this->current_menu = action_menu.parentMenu;
  });

  // Upload opens submenu
  this->addNodes(&action_menu, "Upload", ST77XX_WHITE, NULL, 0, [this]() {
    this->current_menu = &upload_menu;
  });

  this->addNodes(&action_menu, "Delete", ST77XX_WHITE, NULL, 0, [this]() {
    if ("/" + sd_obj.selected_file_name == buffer.getFileName())
      buffer.setFileName("");

    if (sd_obj.removeFile("/" + sd_obj.selected_file_name)) {
      Logger::log(STD_MSG, "Removed file: " + sd_obj.selected_file_name);
      display.clearScreen();
      display.drawCenteredText("File removed", true);
    } else {
      Logger::log(STD_MSG, "Could not remove file");
      display.clearScreen();
      display.drawCenteredText("Could not remove file", true);
    }
    delay(2000);
    this->buildSDFileMenu();
    this->current_menu = &sd_file_menu;
  });

  // Upload Menu
  this->addNodes(&upload_menu, "Back", ST77XX_WHITE, NULL, 0, [this]() {
    this->current_menu = upload_menu.parentMenu;
  });
  this->addNodes(&upload_menu, "WiGLE", ST77XX_WHITE, NULL, 0, [this]() {
    if (wifi_ops.tryConnectToWiFi()) {
      delay(1000);
      if (wifi_ops.uploadFile("/" + sd_obj.selected_file_name, true, WIGLE_UPLOAD)) {
        display.clearScreen();
        display.drawCenteredText("WiGLE OK", true);
      } else {
        display.clearScreen();
        display.drawCenteredText("WiGLE failed", true);
      }
    }
    wifi_ops.deinitWiFi();
    delay(10);
    wifi_ops.initWiFi();
    delay(2000);
    this->current_menu = upload_menu.parentMenu;
  });
  this->addNodes(&upload_menu, "WDGWars", ST77XX_WHITE, NULL, 0, [this]() {
    if (wifi_ops.tryConnectToWiFi()) {
      delay(1000);
      if (wifi_ops.uploadFile("/" + sd_obj.selected_file_name, true, WDG_UPLOAD)) {
        display.clearScreen();
        display.drawCenteredText("WDG OK", true);
      } else {
        display.clearScreen();
        display.drawCenteredText("WDG failed", true);
      }
    }
    wifi_ops.deinitWiFi();
    delay(10);
    wifi_ops.initWiFi();
    delay(2000);
    this->current_menu = upload_menu.parentMenu;
  });
  this->addNodes(&upload_menu, "Both", ST77XX_WHITE, NULL, 0, [this]() {
    if (wifi_ops.tryConnectToWiFi()) {
      delay(1000);
      if (wifi_ops.uploadFile("/" + sd_obj.selected_file_name, true, BOTH_UPLOAD)) {
        display.clearScreen();
        display.drawCenteredText("Upload OK", true);
      } else {
        display.clearScreen();
        display.drawCenteredText("Upload failed", true);
      }
    }
    wifi_ops.deinitWiFi();
    delay(10);
    wifi_ops.initWiFi();
    delay(2000);
    this->current_menu = upload_menu.parentMenu;
  });

  // Mode Menu
  this->addNodes(&mode_menu, "Back", ST77XX_WHITE, NULL, 0, [this]() {
    this->current_menu = mode_menu.parentMenu;
  });
  this->addNodes(&mode_menu, "Solo", ST77XX_WHITE, NULL, 0, [this]() {
    wifi_ops.run_mode = SOLO_MODE;
    this->current_menu = mode_menu.parentMenu;
    display.clearScreen();
    display.drawCenteredText("Mode set", true);
    delay(2000);
  });
  this->addNodes(&mode_menu, "Core", ST77XX_WHITE, NULL, 0, [this]() {
    wifi_ops.run_mode = CORE_MODE;
    this->current_menu = mode_menu.parentMenu;
    display.clearScreen();
    display.drawCenteredText("Mode set", true);
    wifi_ops.startESPNow();
    delay(2000);
  });
  this->addNodes(&mode_menu, "Node", ST77XX_WHITE, NULL, 0, [this]() {
    wifi_ops.run_mode = NODE_MODE;
    this->current_menu = mode_menu.parentMenu;
    display.clearScreen();
    display.drawCenteredText("Mode set", true);
    wifi_ops.startESPNow();
    delay(2000);
  });


  this->current_menu = &sd_file_menu;
  this->init_time    = millis();
}

void UI::printFirmwareVersion() {
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.tft->setCursor(0, 0);
  display.tft->print(FIRMWARE_VERSION);
}

void UI::printBatteryLevel(int8_t batteryLevel) {
  display.tft->setRotation(3);
  display.tft->setTextSize(1);
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  char buf[12];
  snprintf(buf, sizeof(buf), "Bat: %d%%", batteryLevel);

  uint8_t  charWidth = 6;
  uint16_t textWidth = (strlen(buf) + 5) * charWidth;
  uint16_t x         = TFT_WIDTH - textWidth - 2;

  display.tft->setCursor(x, 0);
  if (sd_obj.supported)
    display.tft->setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  else
    display.tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
  display.tft->print("SD");
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  if (battery.i2c_supported) {
    display.tft->print(" | ");
    display.tft->print(buf);
  }
}

// ============================================================
// setDisplayMode — clean mode transition helper
// Resets incognito state, restores backlight, forces redraw
// ============================================================
void UI::setDisplayMode(uint8_t new_mode) {
  if (this->incognito_counting) {
    this->incognito_counting = false;
    display.ctrlBacklight(true);
  }
  this->stat_display_mode      = new_mode;
  this->last_stat_display_mode = 255;
  this->last_mode_change_ms    = millis();
  this->lastUpdateTime         = 0;
  if (new_mode != SD_FILES && new_mode != INCOGNITO)
    display.tft->fillScreen(ST77XX_BLACK);
}

// ============================================================
// Screen 1 — new large-format stats display
// Layout for 160x80px:
//   y=0  : GPS status + battery % + scan status  (size 1)
//   y=19 : divider
//   y=21 : 2.4GHz / 5GHz / BLE labels            (size 1)
//   y=30 : big counts                             (size 2, 16px tall)
//   y=47 : divider
//   y=50 : NET / BLE totals                       (size 2)
//   y=71 : geofence label (only when inside zone) (size 1)
// ============================================================
void UI::drawStatsNew(uint32_t currentTime, uint32_t count2g4, uint32_t count5g,
                      uint32_t bleCount, int gpsSats, int8_t batteryLevel, bool do_now) {

  if ((currentTime - lastUpdateTime < UI_UPDATE_TIME) && (!do_now)) return;
  lastUpdateTime = currentTime;

  display.clearScreen();

  display.tft->setRotation(3);
  display.tft->setTextWrap(false);

  display.tft->setTextSize(1);

  // ---- GPS status (left) ----
  display.tft->setCursor(0, 0);
  bool has_fix = gps.getFixStatus();
  if (has_fix) {
    display.tft->setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    display.tft->print(String(gpsSats) + " sats  ");
  } else {
    display.tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
    display.tft->print("No GPS fix  ");
  }

  // ---- Battery % (right side, row 1) ----
  char batBuf[8];
  snprintf(batBuf, sizeof(batBuf), "%d%%", batteryLevel);
  uint16_t batColor = (batteryLevel > 50) ? ST77XX_GREEN :
                      (batteryLevel > 20) ? ST77XX_YELLOW : ST77XX_RED;
  uint16_t batW = strlen(batBuf) * 6;
  display.tft->setCursor(TFT_WIDTH - batW - 2, 0);
  display.tft->setTextColor(batColor, ST77XX_BLACK);
  display.tft->print(batBuf);

  // ---- Scan status (right side, row 2) ----
  String statusStr;
  uint16_t statusColor;
  if (wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING) {
    statusStr   = "SCANNING";
    statusColor = ST77XX_GREEN;
  } else {
    statusStr   = "STANDBY ";
    statusColor = ST77XX_YELLOW;
  }
  display.tft->setCursor(TFT_WIDTH - statusStr.length() * 6, 9);
  display.tft->setTextColor(statusColor, ST77XX_BLACK);
  display.tft->print(statusStr);

  // ---- Divider ----
  display.tft->drawFastHLine(0, 19, TFT_WIDTH, 0x4208);

  // ---- Column labels ----
  uint16_t col_w = TFT_WIDTH / 3; // 53px each

  display.tft->setTextSize(1);
  display.tft->setTextColor(0x7BEF, ST77XX_BLACK);

  display.tft->setCursor(col_w * 0 + (col_w - 6 * 6) / 2, 21);
  display.tft->print("2.4GHz");
  display.tft->setCursor(col_w * 1 + (col_w - 6 * 4) / 2, 21);
  display.tft->print("5GHz");
  display.tft->setCursor(col_w * 2 + (col_w - 6 * 3) / 2, 21);
  display.tft->print("BLE");

  // ---- Big counts (size 2 = 12x16px per char) ----
  display.tft->setTextSize(2);

  String s24  = String(count2g4);
  String s5   = String(count5g);
  String sble = String(bleCount);

  // Pad with trailing spaces to erase stale wider digits
  while (s24.length()  < 4) s24  += " ";
  while (s5.length()   < 4) s5   += " ";
  while (sble.length() < 4) sble += " ";

  display.tft->setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  display.tft->setCursor(col_w * 0 + 2, 30);
  display.tft->print(s24);

  display.tft->setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  display.tft->setCursor(col_w * 1 + 2, 30);
  display.tft->print(s5);

  display.tft->setTextColor(0xF81F, ST77XX_BLACK); // magenta/purple
  display.tft->setCursor(col_w * 2 + 2, 30);
  display.tft->print(sble);

  // ---- Divider ----
  display.tft->drawFastHLine(0, 47, TFT_WIDTH, 0x4208);

  // ---- Totals (size 2) ----
  display.tft->setTextSize(2);

  String totalNets = String(wifi_ops.getTotalNetCount());
  String totalBLE  = String(wifi_ops.getTotalBLECount());
  while (totalNets.length() < 5) totalNets += " ";
  while (totalBLE.length()  < 5) totalBLE  += " ";

  display.tft->setCursor(0, 50);
  display.tft->setTextColor(0x7BEF, ST77XX_BLACK);
  display.tft->print("W:");
  display.tft->setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  if (wifi_ops.getTotalNetCount() > 9999)
    display.tft->setTextSize(1);
  display.tft->print(totalNets);
  display.tft->setTextSize(2);

  display.tft->setCursor(TFT_WIDTH / 2, 50);
  display.tft->setTextColor(0x7BEF, ST77XX_BLACK);
  display.tft->print("B:");
  display.tft->setTextColor(0xF81F, ST77XX_BLACK);
  if (wifi_ops.getTotalBLECount() > 9999)
    display.tft->setTextSize(1);
  display.tft->print(totalBLE);
  display.tft->setTextSize(2);

  // ---- Geofence label (size 1, bottom row) ----
  display.tft->setTextSize(1);
  display.tft->setCursor(0, 71);
  if (!sd_obj.supported) {
    display.tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
    display.tft->print("NO SD CARD                ");
  } else if (wifi_ops.in_geofence && wifi_ops.current_geo_label.length() > 0) {
    char dist[16];
    display.tft->setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    String geo = "GEO: " + wifi_ops.current_geo_label + " ";
    //while (geo.length() < 26) geo += " ";
    display.tft->print(geo);
    if (wifi_ops.checkGeofences(dist, sizeof(dist)))
      display.tft->print(dist);
  } else {
    display.tft->setTextColor(ST77XX_BLACK, ST77XX_BLACK);
    display.tft->print("                          ");
  }

  if (wifi_ops.run_mode == CORE_MODE) {
    String node_num = "Nodes: " + String(wifi_ops.getNodeCount());
    display.tft->setCursor(TFT_WIDTH - node_num.length() * 6, 80 - 9);
    display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    display.tft->print(node_num);
  }
}

// ============================================================
// Screen 2 — original stats display (unchanged)
// ============================================================
void UI::updateStats(uint32_t currentTime, uint32_t wifiCount, uint32_t count2g4,
                     uint32_t count5g, uint32_t bleCount, int gpsSats,
                     int8_t batteryLevel, bool do_now) {

  if ((currentTime - lastUpdateTime < UI_UPDATE_TIME) && (!do_now)) return;
  lastUpdateTime = currentTime;

  display.clearScreen();

  display.tft->setRotation(3);
  display.tft->setTextWrap(false);

  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.tft->setTextSize(1);

  this->printFirmwareVersion();
  this->printBatteryLevel(batteryLevel);

  display.tft->setCursor(0, 0);
  for (int i = 0; i < 2; i++) display.tft->println();

  if (wifi_ops.getCurrentScanMode() == WIFI_STANDBY)
    display.tft->println("Status: STANDBY ");
  else if (wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING)
    display.tft->println("Status: SCANNING ");

  if (sd_obj.supported)
    display.tft->println("File: " + buffer.getFileName() + "   ");
  if (wifi_ops.run_mode == CORE_MODE)
    display.tft->println("Nodes: " + String(wifi_ops.getNodeCount()) + "   ");

  display.tft->println();

  display.tft->print("2.4GHz: ");
  display.tft->print(String(count2g4) + "   ");
  display.tft->print(" | ");
  display.tft->print("5GHz: ");
  display.tft->println(String(count5g) + "   ");

  display.tft->print("BLE: ");
  display.tft->print(String(bleCount) + "   ");
  display.tft->print(" | GPS Sats: ");
  display.tft->println(gpsSats > 0 ? String(gpsSats) + " " : "No Fix");

  display.tft->println();

  display.tft->setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  display.tft->print("Total Nets: ");
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.tft->println(String(wifi_ops.getTotalNetCount()) + "   ");
  display.tft->setTextColor(CYAN, ST77XX_BLACK);
  display.tft->print("Total BLE: ");
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.tft->println(String(wifi_ops.getTotalBLECount()) + "   ");
}

void UI::setupSDFileList() {
  sd_obj.sd_files->clear();
  delete sd_obj.sd_files;
  sd_obj.sd_files = new LinkedList<String>();
  sd_obj.listDirToLinkedList(sd_obj.sd_files, "/", ".log");
}

void UI::buildSDFileMenu() {
  if (sd_obj.supported) {
    this->setupSDFileList();

    sd_file_menu.list->clear();
    delete sd_file_menu.list;
    sd_file_menu.list = new LinkedList<MenuNode>();
    sd_file_menu.name = "Logs";

    this->addNodes(&sd_file_menu, "Back", ST77XX_WHITE, NULL, 0, [this]() {
      this->setDisplayMode(STATS_NEW);
      if (buffer.getFileName() == "") {
        Logger::log(STD_MSG, "Active log file was deleted. Creating new one...");
        wifi_ops.startLog(LOG_FILE_NAME);
        Logger::log(STD_MSG, "New log file: " + buffer.getFileName());
      }
      this->hard_refresh = true;
    });

    this->addNodes(&sd_file_menu, "Delete Wardrive Logs", ST77XX_WHITE, NULL, 0, [this]() {
      this->current_menu = &delete_all_menu;
    });

    this->addNodes(&sd_file_menu, "Upload All", ST77XX_WHITE, NULL, 0, [this]() {
      this->current_menu = &upload_all_menu;
    });

    this->addNodes(&sd_file_menu, "Mark New Geofence", ST77XX_WHITE, NULL, 0, [this]() {
      this->current_menu = &mark_geofence_menu;
    });

    this->addNodes(&sd_file_menu, "Mode", ST77XX_WHITE, NULL, 0, [this]() {
      this->current_menu = &mode_menu;
    });

    for (int i = 0; i < sd_obj.sd_files->size(); i++) {
      File current_file = sd_obj.getFile("/" + sd_obj.sd_files->get(i));
      if (sd_obj.sd_files->get(i).startsWith("wardrive_") || sd_obj.sd_files->get(i).startsWith("wigle-")) {
        this->addNodes(&sd_file_menu, sd_obj.sd_files->get(i), ST77XX_WHITE, NULL, 0, [this, i]() {
          sd_obj.selected_file_name = sd_obj.sd_files->get(i);
          Logger::log(STD_MSG, sd_obj.sd_files->get(i) + " selected");
          this->current_menu = &action_menu;
        }, current_file.size());
      }
    }

    Logger::log(STD_MSG, "Built SD file menu with " + (String)sd_obj.sd_files->size() + " files");
  } else {
    Logger::log(WARN_MSG, "SD Card not detected. Skipping menu creation...");
  }
}

void UI::addNodes(Menu * menu, String name, uint8_t color, Menu * child, int place,
                  std::function<void()> callable, uint32_t size, bool selected, String command) {
  menu->list->add(MenuNode{name, false, color, place, selected, callable, size});
}

void UI::drawCurrentMenu() {
  if (!current_menu || current_menu->list->size() == 0) return;

  const uint8_t max_visible_items = 7;
  const uint8_t header_height     = 8;

  display.tft->setRotation(3);
  display.tft->fillScreen(ST77XX_BLACK);
  display.tft->setTextSize(1);
  display.tft->setTextWrap(false);

  display.tft->setTextColor(ST77XX_WHITE);
  display.tft->setCursor(0, 0);
  display.tft->println(current_menu->name);

  if (current_menu->selected < current_menu->scroll_offset)
    current_menu->scroll_offset = current_menu->selected;
  else if (current_menu->selected >= current_menu->scroll_offset + max_visible_items)
    current_menu->scroll_offset = current_menu->selected - max_visible_items + 1;

  for (int i = 0; i < max_visible_items; i++) {
    int item_index = current_menu->scroll_offset + i;
    if (item_index >= current_menu->list->size()) break;

    MenuNode node = current_menu->list->get(item_index);
    int y = header_height + i * 8;

    if (item_index == current_menu->selected) {
      display.tft->setTextColor(ST77XX_BLACK, ST77XX_WHITE);
      display.tft->setCursor(0, y);
      display.tft->print("> ");
    } else {
      display.tft->setTextColor(node.color, ST77XX_BLACK);
      display.tft->setCursor(0, y);
      display.tft->print("  ");
    }
    display.tft->print(node.name);

    String sizeStr = "";
    if (node.fileSize > 0) {
      sizeStr  = String((node.fileSize + 1023) / 1024);
      sizeStr += " KB";
    }
    int xRightAlign = TFT_WIDTH - sizeStr.length() * 6;
    display.tft->setCursor(xRightAlign, y);
    display.tft->print(sizeStr);
  }

  if (current_menu->scroll_offset > 0) {
    display.tft->setCursor(TFT_WIDTH - 10, header_height);
    display.tft->setTextColor(ST77XX_WHITE);
    display.tft->print("^");
  }
  if (current_menu->scroll_offset + max_visible_items < current_menu->list->size()) {
    display.tft->setCursor(TFT_WIDTH - 10, header_height + (max_visible_items - 1) * 8);
    display.tft->setTextColor(ST77XX_WHITE);
    display.tft->print("v");
  }
}

void UI::handleMenuNavigation() {
  if (!current_menu || current_menu->list->size() == 0) return;

  int list_size = current_menu->list->size();

  if (u_btn.justPressed()) {
    if (current_menu->selected == 0)
      current_menu->selected = list_size - 1;
    else
      current_menu->selected--;
    drawCurrentMenu();
  }

  if (d_btn.justPressed()) {
    current_menu->selected = (current_menu->selected + 1) % list_size;
    drawCurrentMenu();
  }

  if (c_btn.justPressed()) {
    MenuNode node = current_menu->list->get(current_menu->selected);
    if (node.callable) node.callable();
    drawCurrentMenu();
  }
}

void UI::doHardRefresh() {
  if (this->hard_refresh) {
    Logger::log(STD_MSG, "Hard-refreshing display...");
    display.clearScreen();
    this->hard_refresh = false;
  }
}

void UI::main(uint32_t currentTime) {

  // Handle dock departure display reset
  extern bool g_force_display_redraw;
  if (g_force_display_redraw) {
    this->last_stat_display_mode = 255;
    this->lastUpdateTime         = 0;
    g_force_display_redraw       = false;
    display.tft->fillScreen(ST77XX_BLACK);
  }

  // Don't draw stats while docked — dock mode manages its own display
  if (wifi_ops.isDocked())
    return;

  bool in_stats = (this->stat_display_mode != SD_FILES);

  if (in_stats) {

    // ---- Screen 3: Incognito ----
    if (this->stat_display_mode == INCOGNITO) {

      if (!this->incognito_counting) {
        this->incognito_counting = true;
        this->incognito_start_ms = currentTime;
        this->incognito_last_sec = -1;
        display.tft->fillScreen(ST77XX_BLACK);
        this->last_stat_display_mode = INCOGNITO;
      }

      uint32_t elapsed  = currentTime - this->incognito_start_ms;
      int      secs_rem = (elapsed < 5000) ? (int)(5 - elapsed / 1000) : 0;

      if (elapsed < 5000) {
        if (secs_rem != this->incognito_last_sec) {
          this->incognito_last_sec = secs_rem;
          display.tft->fillScreen(ST77XX_BLACK);
          display.tft->setTextSize(1);
          display.tft->setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
          uint16_t lblX = (TFT_WIDTH - 14 * 6) / 2;
          display.tft->setCursor(lblX > 0 ? lblX : 0, 26);
          display.tft->print("INCOGNITO MODE");
          display.tft->setTextSize(3);
          display.tft->setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
          char buf[3];
          snprintf(buf, sizeof(buf), "%d", secs_rem);
          display.tft->setCursor((TFT_WIDTH - 18) / 2, 44);
          display.tft->print(buf);
        }
      } else {
        if (this->incognito_counting) {
          display.ctrlBacklight(false);
          display.tft->fillScreen(ST77XX_BLACK);
        }
      }

      // Any button press exits incognito — debounced
      if ((u_btn.justPressed() || d_btn.justPressed()) &&
          (currentTime - this->last_mode_change_ms >= 300)) {
        this->setDisplayMode(STATS_NEW);
        display.tft->fillScreen(ST77XX_BLACK);
      }
      return;
    }

    // Leaving incognito — restore backlight
    if (this->incognito_counting) {
      this->incognito_counting = false;
      display.ctrlBacklight(true);
    }

    this->doHardRefresh();

    // ---- Screen 1: new large-format stats ----
    if (this->stat_display_mode == STATS_NEW) {
      this->drawStatsNew(
        currentTime,
        wifi_ops.getCurrent2g4Count(),
        wifi_ops.getCurrent5gCount(),
        wifi_ops.getCurrentBLECount(),
        gps.getNumSats(),
        battery.getBatteryLevel(),
        false
      );
    }
    // ---- Screen 2: original stats ----
    else if (this->stat_display_mode == FULL_STATS) {
      this->updateStats(
        currentTime,
        wifi_ops.getCurrentNetCount(),
        wifi_ops.getCurrent2g4Count(),
        wifi_ops.getCurrent5gCount(),
        wifi_ops.getCurrentBLECount(),
        gps.getNumSats(),
        battery.getBatteryLevel()
      );
    }

    // ---- Button handling — debounced at 300ms ----
    bool mode_change_ok = (currentTime - this->last_mode_change_ms >= 300);

    if (u_btn.justPressed() && mode_change_ok) {
      uint8_t next = (this->stat_display_mode >= MAX_DISPLAY_MODES - 1)
                       ? 0 : this->stat_display_mode + 1;
      this->setDisplayMode(next);
      if (next == SD_FILES)
        this->drawCurrentMenu();
      else if (next == STATS_NEW)
        this->drawStatsNew(currentTime,
          wifi_ops.getCurrent2g4Count(), wifi_ops.getCurrent5gCount(),
          wifi_ops.getCurrentBLECount(), gps.getNumSats(),
          battery.getBatteryLevel(), true);
      else if (next == FULL_STATS)
        this->updateStats(currentTime,
          wifi_ops.getCurrentNetCount(), wifi_ops.getCurrent2g4Count(),
          wifi_ops.getCurrent5gCount(), wifi_ops.getCurrentBLECount(),
          gps.getNumSats(), battery.getBatteryLevel(), true);
    }

    if (d_btn.justPressed() && mode_change_ok) {
      uint8_t next = (this->stat_display_mode == 0)
                       ? MAX_DISPLAY_MODES - 1 : this->stat_display_mode - 1;
      this->setDisplayMode(next);
      if (next == SD_FILES)
        this->drawCurrentMenu();
      else if (next == STATS_NEW)
        this->drawStatsNew(currentTime,
          wifi_ops.getCurrent2g4Count(), wifi_ops.getCurrent5gCount(),
          wifi_ops.getCurrentBLECount(), gps.getNumSats(),
          battery.getBatteryLevel(), true);
      else if (next == FULL_STATS)
        this->updateStats(currentTime,
          wifi_ops.getCurrentNetCount(), wifi_ops.getCurrent2g4Count(),
          wifi_ops.getCurrent5gCount(), wifi_ops.getCurrentBLECount(),
          gps.getNumSats(), battery.getBatteryLevel(), true);
    }

    if (c_btn.justPressed())
      Logger::log(STD_MSG, "C_BTN Pressed: " + (String)millis());

  } else if (this->stat_display_mode == SD_FILES) {
    this->handleMenuNavigation();
  }
}
