#ifndef ui_h
#define ui_h

#include "WiFiOps.h"
#include "display.h"
#include "BatteryInterface.h"
#include "GpsInterface.h"
#include "SDInterface.h"
#include "Buffer.h"
#include "settings.h"
#include "Switches.h"
#include "utils.h"
#include "logger.h"

extern WiFiOps wifi_ops;
extern Display display;
extern BatteryInterface battery;
extern GpsInterface gps;
extern SDInterface sd_obj;
extern Buffer buffer;
extern Settings settings;
extern Utils utils;
extern Switches u_btn;
extern Switches d_btn;
extern Switches c_btn;

#define FULL_STATS 0
#define GLANCE_STATS 1
#define SD_FILES 2

#define MAX_DISPLAY_MODES 3

struct MenuNode {
  String name;
  bool command;
  uint8_t color;
  uint8_t icon;
  bool selected;
  std::function<void()> callable;
  uint32_t fileSize = 0;
};

// Full Menus
struct Menu {
  String name;
  LinkedList<MenuNode>* list;
  Menu                * parentMenu;
  uint16_t               selected = 0;
  uint16_t               scroll_offset = 0;

};

class UI {
  private:
    Menu sd_file_menu;
    Menu mode_menu;
    Menu action_menu;

    uint32_t init_time;
    uint32_t lastUpdateTime = 0;

    void printFirmwareVersion();
    void printBatteryLevel(int8_t batteryLevel);
    void updateStats(uint32_t currentTime, uint32_t wifiCount, uint32_t count2g4, uint32_t count5g, uint32_t bleCount, int gpsSats, int8_t batteryLevel, bool do_now = false);
    void addNodes(Menu * menu, String name, uint8_t color, Menu * child, int place, std::function<void()> callable, uint32_t size = 0, bool selected = false, String command = "");
    void setupSDFileList();
    void buildSDFileMenu();
    void drawCurrentMenu();
    void handleMenuNavigation();

  public:

    Menu* current_menu = nullptr;

    uint8_t stat_display_mode = 0;

    void begin();
    void main(uint32_t currentTime);

};

#endif