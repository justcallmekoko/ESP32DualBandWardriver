#include "display.h"

//Display::Display() {
//
//}

Display::Display(SPIClass* spi, int cs, int dc, int rst)
  : _spi(spi), _cs(cs), _dc(dc), _rst(rst) {
    tft = new Adafruit_ST7735(_spi, _cs, _dc, _rst);
}

void Display::begin() {
  pinMode(TFT_BL, OUTPUT);
  
  this->ctrlBacklight(false);

  //tft.init();
  tft->initR(INITR_MINI160x80_PLUGIN);

  tft->setSPISpeed(TFT_SPI_SPEED);

  this->clearScreen();
  
  tft->setTextWrap(false);

  tft->setRotation(3);

  //tft.drawCentreString(DEVICE_NAME, TFT_WIDTH / 2, TFT_HEIGHT * 0.33, 1);
  //tft.drawCentreString("JCMK", TFT_WIDTH / 2, TFT_HEIGHT * 0.5, 1);
  //tft.drawCentreString(FIRMWARE_VERSION, TFT_WIDTH / 2, TFT_HEIGHT * 0.66, 1);
  tft->println("JCMK C5 Wardriver");

  this->ctrlBacklight(true);
}

void Display::ctrlBacklight(bool on) {
  if (on)
    digitalWrite(TFT_BL, ON);
  else
    digitalWrite(TFT_BL, OFF);
}

void Display::clearScreen() {
  tft->fillScreen(ST77XX_BLACK);
}

void Display::main(uint32_t currentTime) {

}