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
#include "ui.h"
#include "logger.h"

Buffer buffer;
Settings settings;
GpsInterface gps;
BatteryInterface battery;
WiFiOps wifi_ops;
Utils utils;
UI ui_obj;

SPIClass sharedSPI(SPI);
Display display = Display(&sharedSPI, TFT_CS, TFT_DC, TFT_RST);
SDInterface sd_obj = SDInterface(&sharedSPI, SD_CS);

Switches u_btn = Switches(U_BTN, 1000, U_PULL);
Switches d_btn = Switches(D_BTN, 1000, D_PULL);
Switches c_btn = Switches(C_BTN, 1000, C_PULL);

void setup() {
  Serial.begin(115200);

  while (!Serial)
    delay(10);

  // Do SPI stuff first
  sharedSPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // Give SPI some time I guess
  delay(100);

  // Init the display before SD
  display.begin();

  // Give SD some time
  delay(100);

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

  // Check for firmware updates now
  Logger::log(STD_MSG, "Checking for firmware updates...");
  sd_obj.runUpdate();

  // Init battery
  battery.RunSetup();
  battery.battery_level = battery.getBatteryLevel();

  // Init GPS
  gps.begin();

  // Init wifi and bluetooth
  wifi_ops.begin();

  // Init UI
  ui_obj.begin();

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
  ui_obj.main(currentTime);

  if ((gps.getFixStatus()) && (sd_obj.supported))
    wifi_ops.setCurrentScanMode(WIFI_WARDRIVING);
  else
    wifi_ops.setCurrentScanMode(WIFI_STANDBY);
}
