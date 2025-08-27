#include "ui.h"

void UI::begin() {
  sd_file_menu.list = new LinkedList<MenuNode>();
  action_menu.list = new LinkedList<MenuNode>();

  action_menu.name = "Action";

  this->buildSDFileMenu();

  action_menu.parentMenu = &sd_file_menu;

  this->addNodes(&action_menu, "Back", ST77XX_WHITE, NULL, 0, [this]() {
    this->current_menu = action_menu.parentMenu;
  });
  this->addNodes(&action_menu, "Upload", ST77XX_WHITE, NULL, 0, [this]() {
    //wifi_ops.deinitWiFi();
    //delay(10);
    //wifi_ops.initWiFi();
    //delay(10);

    //wifi_ops.deinitBLE();

    if (wifi_ops.tryConnectToWiFi()) {
      delay(1000);
      if (wifi_ops.backendUpload("/" + sd_obj.selected_file_name)) {
        display.clearScreen();
        display.drawCenteredText("Upload complete", true);
        buffer.setFileName("");
      }
    }
    
    wifi_ops.deinitWiFi();
    delay(10);
    wifi_ops.initWiFi();
    //wifi_ops.initBLE();
    delay(2000);
  });
  this->addNodes(&action_menu, "Delete", ST77XX_WHITE, NULL, 0, [this]() {
    if ("/" + sd_obj.selected_file_name == buffer.getFileName())
      buffer.setFileName("");

    if (sd_obj.removeFile("/" + sd_obj.selected_file_name)) {
      Logger::log(STD_MSG, "Removed file: " + sd_obj.selected_file_name);

      display.clearScreen();

      display.drawCenteredText("File removed", true);
    }
    else {
      Logger::log(STD_MSG, "Could not remove file");

      display.clearScreen();

      display.drawCenteredText("Could not remove file", true);
    }

    delay(2000);

    this->buildSDFileMenu();

    this->current_menu = &sd_file_menu;
  });

  this->current_menu = &sd_file_menu;

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
      display.tft->println("Status: STANDBY");
    else if (wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING)
      display.tft->println("Status: SCANNING");

    if (sd_obj.supported)
      display.tft->println("File: " + buffer.getFileName());

    display.tft->println();


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

  sd_file_menu.list->clear();
  delete sd_file_menu.list;
  sd_file_menu.list = new LinkedList<MenuNode>();
  sd_file_menu.name = "Logs";

  this->addNodes(&sd_file_menu, "Back", ST77XX_WHITE, NULL, 0, [this]() {
    this->stat_display_mode = 0;

    if (buffer.getFileName() == "") {
      Logger::log(STD_MSG, "Active log file was deleted. Creating new one...");
      wifi_ops.startLog(LOG_FILE_NAME);
      Logger::log(STD_MSG, "New log file: " + buffer.getFileName());
    }
  });

  for (int i = 0; i < sd_obj.sd_files->size(); i++) {
    File current_file = sd_obj.getFile("/" + sd_obj.sd_files->get(i));
    this->addNodes(&sd_file_menu, sd_obj.sd_files->get(i), ST77XX_WHITE, NULL, 0, [this, i]() {
      sd_obj.selected_file_name = sd_obj.sd_files->get(i);
      Logger::log(STD_MSG, sd_obj.sd_files->get(i) + " selected");
      this->current_menu = &action_menu;
    }, current_file.size());
  }

  Logger::log(STD_MSG, "Built SD file menu with " + (String)sd_obj.sd_files->size() + " files");
}

void UI::addNodes(Menu * menu, String name, uint8_t color, Menu * child, int place, std::function<void()> callable, uint32_t size, bool selected, String command) {
  menu->list->add(MenuNode{name, false, color, place, selected, callable, size});
}

void UI::drawCurrentMenu() {
  if (!current_menu || current_menu->list->size() == 0) return;

  const uint8_t max_visible_items = 7;
  const uint8_t header_height = 8;

  display.tft->setRotation(3);
  display.tft->fillScreen(ST77XX_BLACK);
  display.tft->setTextSize(1);
  display.tft->setTextWrap(false);

  display.tft->setTextColor(ST77XX_WHITE);
  display.tft->setCursor(0, 0);
  display.tft->println(current_menu->name);

  // Update scroll offset
  if (current_menu->selected < current_menu->scroll_offset)
    current_menu->scroll_offset = current_menu->selected;
  else if (current_menu->selected >= current_menu->scroll_offset + max_visible_items)
    current_menu->scroll_offset = current_menu->selected - max_visible_items + 1;

  // Draw visible items
  for (int i = 0; i < max_visible_items; i++) {
    int item_index = current_menu->scroll_offset + i;
    if (item_index >= current_menu->list->size()) break;

    MenuNode node = current_menu->list->get(item_index);
    int y = header_height + i * 8;

    // Handle selection color
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
      sizeStr = String((node.fileSize + 1023) / 1024);  // Round up
      sizeStr += " KB";
    }

    // Align file size to the right
    int textPixelWidth = sizeStr.length() * 6;
    int xRightAlign = TFT_WIDTH - textPixelWidth;
    display.tft->setCursor(xRightAlign, y);
    display.tft->print(sizeStr);
  }

  // Scroll indicators
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

  // Up
  if (u_btn.justPressed()) {
    if (current_menu->selected == 0)
      current_menu->selected = list_size - 1;
    else
      current_menu->selected--;

    drawCurrentMenu();
  }

  // Down
  if (d_btn.justPressed()) {
    current_menu->selected = (current_menu->selected + 1) % list_size;
    drawCurrentMenu();
  }

  // Select
  if (c_btn.justPressed()) {
    MenuNode node = current_menu->list->get(current_menu->selected);
    if (node.callable)
      node.callable();

    drawCurrentMenu();  // Refresh in case menu changed
  }
}

void UI::main(uint32_t currentTime) {
  if (((wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING) || (wifi_ops.getCurrentScanMode() == WIFI_STANDBY)) && (this->stat_display_mode != SD_FILES)) {
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

      if (this->stat_display_mode == SD_FILES)
        this->drawCurrentMenu();

      if (((wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING) || (wifi_ops.getCurrentScanMode() == WIFI_STANDBY)) && (this->stat_display_mode != SD_FILES))
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

      if (this->stat_display_mode == SD_FILES)
        this->drawCurrentMenu();

      if (((wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING) || (wifi_ops.getCurrentScanMode() == WIFI_STANDBY)) && (this->stat_display_mode != SD_FILES))
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
  else if (this->stat_display_mode == SD_FILES) {
    this->handleMenuNavigation();
  }
}