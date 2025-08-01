#include "ui.h"

void UI::begin() {
  sd_file_menu.list = new LinkedList<MenuNode>();

  this->buildSDFileMenu();

  this->init_time = millis();
}

void UI::printFirmwareVersion() {
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  const char* version = FIRMWARE_VERSION;

  display.tft->setCursor(0, 0);
  display.tft->print(version);
}

void UI::printBatteryLevel(int8_t batteryLevel) {
    display.tft->setRotation(3);  // Landscape
    display.tft->setTextSize(1);  // 6px per char
    display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);

    // Format battery string
    char buf[12];
    snprintf(buf, sizeof(buf), "Bat: %d%%", batteryLevel);

    // Compute text width and cursor position
    uint8_t charWidth = 6;
    uint16_t textWidth = (strlen(buf) + 5) * charWidth;
    uint16_t x = TFT_WIDTH - textWidth - 2;  // Right-aligned with 2px padding
    uint16_t y = 0;  // Adjust based on layout

    display.tft->setCursor(x, y);
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

void UI::updateStats(uint32_t currentTime, uint32_t wifiCount, uint32_t count2g4, uint32_t count5g, uint32_t bleCount, int gpsSats, int8_t batteryLevel, bool do_now) {
  if ((currentTime - lastUpdateTime < UI_UPDATE_TIME) && (!do_now)) return;
  lastUpdateTime = currentTime;

  display.tft->setRotation(3);  // Landscape mode
  display.tft->fillRect(0, 0, 160, 80, ST77XX_BLACK);  // Clear top half

  display.tft->setTextColor(ST77XX_WHITE);
  display.tft->setTextSize(1);

  this->printFirmwareVersion();
  this->printBatteryLevel(batteryLevel);

  if (this->stat_display_mode == FULL_STATS) {

    display.tft->setCursor(0, 0);

    for (int i = 0; i < 2; i++)
      display.tft->println();

    if (wifi_ops.getCurrentScanMode() == WIFI_STANDBY)
      display.tft->println("Status: STANDBY\n");
    else if (wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING)
      display.tft->println("Status: SCANNING\n");


    display.tft->print("2.4GHz: ");
    display.tft->print(count2g4);
    display.tft->print(" | ");
    display.tft->print("5GHz: ");
    display.tft->println(count5g);

    display.tft->print("BLE: ");
    display.tft->print(bleCount);
    display.tft->print(" | GPS Sats: ");
    display.tft->println(gpsSats > 0 ? String(gpsSats) : "No Fix");

    display.tft->println();

    display.tft->setTextColor(ST77XX_GREEN);
    display.tft->print("Total Nets: ");
    display.tft->setTextColor(ST77XX_WHITE);
    display.tft->println(wifi_ops.getTotalNetCount());
    display.tft->setTextColor(CYAN);
    display.tft->print("Total BLE: ");
    display.tft->setTextColor(ST77XX_WHITE);
    display.tft->println(wifi_ops.getTotalBLECount());
    display.tft->setTextColor(ST77XX_WHITE);
  }
  else if (this->stat_display_mode == GLANCE_STATS) {
    for (int i = 0; i < 2; i++)
      display.tft->println();

    display.tft->print("GPS Sats: ");
    display.tft->println(gpsSats > 0 ? String(gpsSats) : "No Fix");

    display.tft->setTextSize(2);

    display.tft->println();

    display.tft->setTextColor(ST77XX_GREEN);
    //display.tft->print("Total Nets: ");
    //display.tft->setTextColor(ST77XX_WHITE);
    display.tft->println(wifi_ops.getTotalNetCount());
    display.tft->setTextColor(CYAN);
    //display.tft->print("Total BLE: ");
    //display.tft->setTextColor(ST77XX_WHITE);
    display.tft->println(wifi_ops.getTotalBLECount());
    display.tft->setTextColor(ST77XX_WHITE);
    
    display.tft->setTextSize(1);
  }
}

void UI::setupSDFileList() {
  sd_obj.sd_files->clear();
  delete sd_obj.sd_files;

  sd_obj.sd_files = new LinkedList<String>();

  //sd_obj.sd_files->add("Back");

  sd_obj.listDirToLinkedList(sd_obj.sd_files, "/", ".log");
}

void UI::buildSDFileMenu() {
  this->setupSDFileList();

  this->sd_file_menu.list->clear();
  delete this->sd_file_menu.list;
  this->sd_file_menu.list = new LinkedList<MenuNode>();
  this->sd_file_menu.name = "Logs";

  for (int i = 0; i < sd_obj.sd_files->size(); i++) {
    this->addNodes(&sd_file_menu, sd_obj.sd_files->get(i), ST77XX_WHITE, NULL, 0, [this, i]() {
      Logger::log(STD_MSG, sd_obj.sd_files->get(i) + " selected");
    });
  }

  Logger::log(STD_MSG, "Built SD file menu with " + (String)sd_obj.sd_files->size() + " files");
}

void UI::addNodes(Menu * menu, String name, uint8_t color, Menu * child, int place, std::function<void()> callable, bool selected, String command)
{
  menu->list->add(MenuNode{name, false, color, place, selected, callable});
  //menu->list->add(MenuNode{name, false, color, place, selected, callable});
}

void UI::main(uint32_t currentTime) {
  if ((wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING) || (wifi_ops.getCurrentScanMode() == WIFI_STANDBY))
    this->updateStats(
      currentTime,
      wifi_ops.getCurrentNetCount(),
      wifi_ops.getCurrent2g4Count(),
      wifi_ops.getCurrent5gCount(),
      wifi_ops.getCurrentBLECount(),
      gps.getNumSats(),
      battery.getBatteryLevel()
    );

  if (u_btn.justPressed()) {
    if (this->stat_display_mode >= MAX_DISPLAY_MODES - 1)
      this->stat_display_mode = 0;
    else
      this->stat_display_mode++;

    if ((wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING) || (wifi_ops.getCurrentScanMode() == WIFI_STANDBY))
      this->updateStats(
        currentTime,
        wifi_ops.getCurrentNetCount(),
        wifi_ops.getCurrent2g4Count(),
        wifi_ops.getCurrent5gCount(),
        wifi_ops.getCurrentBLECount(),
        gps.getNumSats(),
        battery.getBatteryLevel(),
        true
      );
  }

  if (d_btn.justPressed()) {
    if (this->stat_display_mode <= 0)
      this->stat_display_mode = MAX_DISPLAY_MODES - 1;
    else
      this->stat_display_mode--;

    if ((wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING) || (wifi_ops.getCurrentScanMode() == WIFI_STANDBY))
      this->updateStats(
        currentTime,
        wifi_ops.getCurrentNetCount(),
        wifi_ops.getCurrent2g4Count(),
        wifi_ops.getCurrent5gCount(),
        wifi_ops.getCurrentBLECount(),
        gps.getNumSats(),
        battery.getBatteryLevel(),
        true
      );
  }


  if (c_btn.justPressed())
    Logger::log(STD_MSG, "C_BTN Pressed: " + (String)millis());
}