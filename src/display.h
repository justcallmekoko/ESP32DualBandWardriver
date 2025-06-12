#ifndef display_h
#define display_h

#include <FS.h>
#include <LinkedList.h>
#include <TFT_eSPI.h>
#include <functional>

#include "configs.h"

#include "BatteryInterface.h"

extern BatteryInterface battery;

class Display {
  public:
    Display();
    TFT_eSPI tft = TFT_eSPI();

    void begin();
    void main(uint32_t currentTime);
    void clearScreen();
    void ctrlBacklight(bool on = true);

  private:

};

#endif