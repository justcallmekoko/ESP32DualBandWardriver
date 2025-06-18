#ifndef display_h
#define display_h

#include <FS.h>
#include <LinkedList.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <functional>

#include "configs.h"
#include "assets.h"

#include "BatteryInterface.h"

extern BatteryInterface battery;

class Display {
  public:
    int _cs, _dc, _rst;
    Display(SPIClass* spi, int cs, int dc, int rst);
    Adafruit_ST7735* tft;

    void begin();
    void main(uint32_t currentTime);
    void clearScreen();
    void ctrlBacklight(bool on = true);

  private:
    SPIClass* _spi;

    void drawMonochromeImage160x80(const uint8_t* imageData, int width, int height);

};

#endif