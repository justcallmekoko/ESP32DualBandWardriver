#ifndef ui_h
#define ui_h

#include "WiFiOps.h"
#include "display.h"
#include "BatteryInterface.h"
#include "GpsInterface.h"
#include "SDInterface.h"
#include "settings.h"
#include "Switches.h"
#include "utils.h"
#include "logger.h"

extern WiFiOps wifi_ops;
extern Display display;
extern BatteryInterface battery;
extern GpsInterface gps;
extern SDInterface sd_obj;
extern Settings settings;
extern Utils utils;
extern Switches u_btn;
extern Switches d_btn;
extern Switches c_btn;

#define FULL_STATS 0
#define GLANCE_STATS 1

#define MAX_DISPLAY_MODES 2

class UI {
  private:
    uint32_t init_time;
    uint32_t lastUpdateTime = 0;
    uint8_t stat_display_mode = 0;

    void printFirmwareVersion();
    void printBatteryLevel(int8_t batteryLevel);
    void updateStats(uint32_t currentTime, uint32_t wifiCount, uint32_t count2g4, uint32_t count5g, uint32_t bleCount, int gpsSats, int8_t batteryLevel, bool do_now = false);

  public:
    void begin();
    void main(uint32_t currentTime);

};

#endif