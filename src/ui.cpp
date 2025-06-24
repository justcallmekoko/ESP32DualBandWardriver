#include "ui.h"

void UI::begin() {
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

void UI::updateStats(uint32_t currentTime, uint32_t wifiCount, uint32_t count2g4, uint32_t count5g, uint32_t bleCount, int gpsSats, int8_t batteryLevel) {
  if (currentTime - lastUpdateTime < UI_UPDATE_TIME) return;
  lastUpdateTime = currentTime;

  display.tft->setRotation(3);  // Landscape mode
  display.tft->fillRect(0, 0, 160, 80, ST77XX_BLACK);  // Clear top half

  display.tft->setTextColor(ST77XX_WHITE);
  display.tft->setTextSize(1);

  //display.tft->print("WiFi: ");
  //display.tft->print(wifiCount);

  this->printFirmwareVersion();
  this->printBatteryLevel(batteryLevel);

  display.tft->setCursor(0, 0);

  for (int i = 0; i < 2; i++)
    display.tft->println();

  if (wifi_ops.getCurrentScanMode() == WIFI_STANDBY)
    display.tft->println("Status: STANDBY\n");
  else if (wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING)
    display.tft->println("Status: SCANNING\n");


  //display.tft->setTextColor(ST77XX_GREEN);
  display.tft->print("2.4GHz: ");
  //display.tft->setTextColor(ST77XX_WHITE);
  display.tft->print(count2g4);
  display.tft->print(" | ");

  //display.tft->setTextColor(ST77XX_GREEN);
  display.tft->print("5GHz: ");
  //display.tft->setTextColor(ST77XX_WHITE);
  display.tft->println(count5g);

  //display.tft->println();

  //display.tft->setTextColor(ST77XX_CYAN);
  display.tft->print("BLE: ");
  //display.tft->setTextColor(ST77XX_WHITE);
  display.tft->print(bleCount);

  display.tft->print(" | GPS Sats: ");
  display.tft->println(gpsSats > 0 ? String(gpsSats) : "No Fix");

  display.tft->println();

  display.tft->setTextColor(ST77XX_GREEN);
  display.tft->print("Total Nets: ");
  display.tft->setTextColor(ST77XX_WHITE);
  display.tft->println(wifi_ops.getTotalNetCount());
  display.tft->setTextColor(ST77XX_CYAN);
  display.tft->print("Total BLE: ");
  display.tft->setTextColor(ST77XX_WHITE);
  display.tft->println(wifi_ops.getTotalBLECount());
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

  if (u_btn.justPressed())
    Logger::log(STD_MSG, "U_BTN Pressed: " + (String)millis());

  if (d_btn.justPressed())
    Logger::log(STD_MSG, "D_BTN Pressed: " + (String)millis());

  if (c_btn.justPressed())
    Logger::log(STD_MSG, "C_BTN Pressed: " + (String)millis());
}