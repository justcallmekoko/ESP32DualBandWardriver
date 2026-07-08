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

// ============================================================
// Display modes — cycle with UP/DOWN buttons
// ============================================================
#define STATS_NEW    0  // New large-format stats screen (Screen 1)
#define FULL_STATS   1  // Original stats screen (Screen 2)
#define SD_FILES     2  // SD file menu
#define INCOGNITO    3  // Blank screen (Screen 3)

#define MAX_DISPLAY_MODES 4

struct MenuNode {
  String name;
  bool command;
  uint8_t color;
  uint8_t icon;
  bool selected;
  std::function<void()> callable;
  uint32_t fileSize = 0;
};

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
    Menu upload_menu;
    Menu delete_all_menu;
    Menu upload_all_menu;
    Menu mark_geofence_menu;
    

    bool hard_refresh = false;

    uint32_t init_time;
    uint32_t lastUpdateTime         = 0;
    uint32_t last_mode_change_ms    = 0; // debounce rapid button pushes
    uint8_t  last_stat_display_mode = 255; // forces clear on first draw

    // Incognito countdown state
    bool     incognito_counting = false;
    uint32_t incognito_start_ms = 0;
    int      incognito_last_sec = -1;

    void printFirmwareVersion();
    void printBatteryLevel(int8_t batteryLevel);
    void drawStatsNew(uint32_t currentTime, uint32_t count2g4, uint32_t count5g,
                      uint32_t bleCount, int gpsSats, int8_t batteryLevel, bool do_now);
    void updateStats(uint32_t currentTime, uint32_t wifiCount, uint32_t count2g4,
                     uint32_t count5g, uint32_t bleCount, int gpsSats,
                     int8_t batteryLevel, bool do_now = false);
    void setDisplayMode(uint8_t new_mode);
    void addNodes(Menu * menu, String name, uint8_t color, Menu * child, int place,
                  std::function<void()> callable, uint32_t size = 0,
                  bool selected = false, String command = "");
    void setupSDFileList();
    void buildSDFileMenu();
    void drawCurrentMenu();
    void handleMenuNavigation();

    void doHardRefresh();

  public:
    Menu* current_menu = nullptr;
    uint8_t stat_display_mode = 0;

    void begin();
    void main(uint32_t currentTime);
};

#endif
