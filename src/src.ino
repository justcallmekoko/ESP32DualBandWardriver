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

// ============================================================
// Chunk 7: Power-off timer globals
// ============================================================
bool     g_was_charging        = true;   // assume USB present at boot
bool     g_poweroff_active     = false;  // true while countdown is running
uint32_t g_poweroff_start_ms   = 0;      // millis() when USB was removed
uint32_t g_poweroff_last_tft   = 0;      // rate-limit TFT countdown redraws
int      g_poweroff_last_secs  = -1;     // last displayed second value

// ============================================================

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

  // Init battery
  battery.RunSetup();
  battery.battery_level = battery.getBatteryLevel();

  // Chunk 7: seed initial USB state from the battery IC
  g_was_charging    = battery.isCharging();
  g_poweroff_active = false;

  // Init GPS
  gps.begin();

  ui_obj.begin();

  // Init wifi and bluetooth
  wifi_ops.begin(c_btn.justPressed());

  // Init UI
  ui_obj.begin();

  settings.printJsonSettings(settings.getSettingsString());

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

  // ============================================================
  // Chunk 7: Power-off timer
  // ============================================================
  bool poweroff_enabled = settings.loadSetting<bool>(POWEROFF_EN_NAME);

  if (poweroff_enabled) {
    bool now_charging = battery.isCharging();

    // --- Detect USB disconnect ---
    if (g_was_charging && !now_charging && !g_poweroff_active) {
      g_poweroff_active   = true;
      g_poweroff_start_ms = currentTime;
      g_poweroff_last_secs = -1;
      int mins = settings.loadSetting<int>(POWEROFF_MIN_NAME);
      if (mins < 1 || mins > 60) mins = POWEROFF_DEFAULT_MINS;
      Logger::log(WARN_MSG, "[PWR] USB removed — power-off in " +
                  String(mins) + " min");
    }

    // --- USB reconnected — cancel countdown ---
    if (!g_was_charging && now_charging && g_poweroff_active) {
      g_poweroff_active = false;
      Logger::log(GUD_MSG, "[PWR] USB restored — power-off cancelled");
      display.clearScreen(); // let normal UI reclaim TFT
    }

    g_was_charging = now_charging;

    // --- Countdown running ---
    if (g_poweroff_active) {
      int mins = settings.loadSetting<int>(POWEROFF_MIN_NAME);
      if (mins < 1 || mins > 60) mins = POWEROFF_DEFAULT_MINS;
      uint32_t total_ms  = (uint32_t)mins * 60UL * 1000UL;
      uint32_t elapsed   = currentTime - g_poweroff_start_ms;

      if (elapsed >= total_ms) {
        // --- Timer expired: shut down ---
        Logger::log(WARN_MSG, "[PWR] Power-off timer expired — shutting down");

        // Flush write buffer to SD so no data is lost
        buffer.save();

        // Show shutdown message on TFT
        display.clearScreen();
        display.tft->setCursor(0, 0);
        display.tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
        display.tft->println("SHUTTING DOWN");
        display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        display.tft->println("Data saved.");
        display.tft->println("Reconnect USB");
        display.tft->println("to restart.");
        delay(1500);

        // Deep sleep with no wakeup source.
        // Device wakes only on hard reset (USB reconnect resets ESP32).
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        esp_deep_sleep_start();

        // Never reached
        return;

      } else {
        // --- Update TFT countdown (max once per second) ---
        uint32_t remaining_ms   = total_ms - elapsed;
        int      remaining_secs = (int)(remaining_ms / 1000);

        if (remaining_secs != g_poweroff_last_secs &&
            currentTime - g_poweroff_last_tft >= 1000) {

          g_poweroff_last_secs = remaining_secs;
          g_poweroff_last_tft  = currentTime;

          int disp_min = remaining_secs / 60;
          int disp_sec = remaining_secs % 60;

          display.clearScreen();
          display.tft->setCursor(0, 0);
          display.tft->setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
          display.tft->println("USB REMOVED");
          display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
          display.tft->println("Shutting down in:");
          display.tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
          char t[9];
          snprintf(t, sizeof(t), "%02d:%02d", disp_min, disp_sec);
          display.tft->println(String(t));
          display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
          display.tft->println("Reconnect USB");
          display.tft->println("to cancel.");

          Logger::log(STD_MSG, "[PWR] Shutdown in " + String(t));
        }
      }
    }
  }
  // ============================================================

  // Solo or Core modes
  if ((gps.getFixStatus()) && (sd_obj.supported) && (ui_obj.stat_display_mode != SD_FILES))
    wifi_ops.setCurrentScanMode(WIFI_WARDRIVING);
  // Nodes
  else if ((wifi_ops.run_mode == NODE_MODE) && (wifi_ops.getNodeReady())) {
    wifi_ops.setCurrentScanMode(WIFI_WARDRIVING);
    digitalWrite(LED_PIN, HIGH);
  }
  else {
    wifi_ops.setCurrentScanMode(WIFI_STANDBY);
    if (wifi_ops.run_mode == NODE_MODE)
      digitalWrite(LED_PIN, LOW);
  }
}

