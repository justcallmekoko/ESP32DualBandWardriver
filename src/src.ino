#include "configs.h"
#include "BatteryInterface.h"
#include "Buffer.h"
#include "settings.h"
#include "display.h"
#include "GpsInterface.h"
#include "SDInterface.h"
#include "Switches.h"
#include "WiFiOps.h"
#include "utils.h"
#include "logger.h"

Buffer buffer;
Settings settings;
GpsInterface gps;
BatteryInterface battery;
Display display;
SDInterface sd_obj;
WiFiOps wifi_ops;
Utils utils;

void setup() {
  Serial.begin(115200);

  while (!Serial)
    delay(10);

  // Init the display before SD
  //display.begin();

  // Show us IDF information
  Logger::log(STD_MSG, "ESP-IDF version is: " + String(esp_get_idf_version()));

  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);

  // Load settings
  settings.begin();

  // Init our buffer for writing logs
  buffer = Buffer();

  // Init SD Card
  if(!sd_obj.initSD())
    Logger::log(WARN_MSG, "SD Card NOT Supported");

  // Init battery
  battery.RunSetup();
  battery.battery_level = battery.getBatteryLevel();

  // Init GPS
  gps.begin();

  wifi_ops.begin();

  Logger::log(GUD_MSG, "Initialization complete!");

}

void loop() {
  // Take current time of this loop for functions
  uint32_t currentTime = millis();

  // Refresh all functions
  wifi_ops.main(currentTime);
  settings.main(currentTime);
  battery.main(currentTime);
  gps.main();
  sd_obj.main();
  buffer.save();

  if ((gps.getFixStatus()) && (sd_obj.supported))
    wifi_ops.setCurrentScanMode(WIFI_WARDRIVING);
  else
    wifi_ops.setCurrentScanMode(WIFI_STANDBY);
}
