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
#ifdef HAS_T_DONGLE_LED
  #include <APA102.h>
#endif

Buffer buffer;
Settings settings;
GpsInterface gps;
BatteryInterface battery;
WiFiOps wifi_ops;
Utils utils;
UI ui_obj;
bool g_force_display_redraw = false;
SPIClass sharedSPI(SPI);

#ifdef HAS_T_DONGLE_LED
APA102<T_DONGLE_LED_DATA_PIN, T_DONGLE_LED_CLOCK_PIN> t_dongle_led;

void writeTDongleLed(bool scanning) {
  const uint8_t brightness = scanning ? 10 : 0;
  t_dongle_led.startFrame();
  t_dongle_led.sendColor(0, 0, scanning ? 255 : 0, brightness);
  t_dongle_led.endFrame(1);
}

void restoreTDongleSpi() {
  // The APA102 and TFT share MOSI/clock. The software-driven LED frame takes
  // ownership of those pins, so restore the hardware SPI routing before any
  // display or SD work in the next loop.
  sharedSPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
}
#endif

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

  #ifdef HAS_ACTIVITY_LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
  #endif

  // Load settings
  settings.begin();

  if (settings.getSettingType(SETTING_SANITY) == "") {
    Logger::log(WARN_MSG, "Current settings format not supported. Installing new default settings...");
    settings.createDefaultSettings(SPIFFS);
  }
  else {
    Logger::log(GUD_MSG, "Current settings format supported");
  }

  // Init our buffer for writing logs
  buffer = Buffer();

  // Init SD Card
  if(!sd_obj.initSD())
    Logger::log(WARN_MSG, "SD Card NOT Supported");

  // Check for firmware updates now
  Logger::log(STD_MSG, "Checking for firmware updates...");
  sd_obj.runUpdate();

  // Enable SD debug logging if setting is on
  Logger::enableSDLog(settings.loadSetting<bool>(DEBUG_LOG_NAME));

  // Init battery
  battery.RunSetup();
  battery.battery_level = battery.getBatteryLevel();

  // Init GPS
  gps.begin();

  ui_obj.begin();

  // Init wifi and bluetooth
  wifi_ops.begin(c_btn.justPressed());

  // Init UI
  ui_obj.begin();

  settings.printJsonSettings(settings.getSettingsString());

  Logger::log(GUD_MSG, "Initialization complete!");

  #ifdef HAS_T_DONGLE_LED
    // Setup performs several display and SD transactions. Leave the shared
    // pins with the LED last and explicitly latch the idle/off state.
    pinMode(T_DONGLE_LED_DATA_PIN, OUTPUT);
    pinMode(T_DONGLE_LED_CLOCK_PIN, OUTPUT);
    writeTDongleLed(false);
  #endif
}

void loop() {
  // Take current time of this loop for functions
  uint32_t currentTime = millis();

  #ifdef HAS_T_DONGLE_LED
    restoreTDongleSpi();
  #endif

  // Refresh all functions
  wifi_ops.main(currentTime, ui_obj.stat_display_mode == SD_FILES);
  settings.main(currentTime);
  battery.main(currentTime);
  gps.main();
  sd_obj.main();
  buffer.save();
  ui_obj.main(currentTime);

  // Solo or Core modes
  if ((gps.getFixStatus()) && (sd_obj.supported) && (ui_obj.stat_display_mode != SD_FILES))
    wifi_ops.setCurrentScanMode(WIFI_WARDRIVING);
  // Nodes
  else if ((wifi_ops.run_mode == NODE_MODE) && (wifi_ops.getNodeReady())) {
    wifi_ops.setCurrentScanMode(WIFI_WARDRIVING);
    #ifdef HAS_ACTIVITY_LED
      digitalWrite(LED_PIN, HIGH);
    #endif
  }
  else {
    wifi_ops.setCurrentScanMode(WIFI_STANDBY);
    if (wifi_ops.run_mode == NODE_MODE) {
      #ifdef HAS_ACTIVITY_LED
        digitalWrite(LED_PIN, LOW);
      #endif
    }
  }

  #ifdef HAS_T_DONGLE_LED
    // TFT and SD traffic on GPIO2/GPIO6 looks like LED data. Reassert the
    // intended state last on every loop: blue while scanning, off at idle.
    writeTDongleLed(wifi_ops.getCurrentScanMode() == WIFI_WARDRIVING);
  #endif
}
