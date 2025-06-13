#include "display.h"

Display::Display() {
  
}

void Display::begin() {
  pinMode(TFT_BL, OUTPUT);
  
  this->ctrlBacklight(false);

  tft.init();
  this->clearScreen();
  
  tft.setTextWrap(false);

  tft.setRotation(3);

  tft.drawCentreString(DEVICE_NAME, TFT_WIDTH / 2, TFT_HEIGHT * 0.33, 1);
  tft.drawCentreString("JCMK", TFT_WIDTH / 2, TFT_HEIGHT * 0.5, 1);
  tft.drawCentreString(FIRMWARE_VERSION, TFT_WIDTH / 2, TFT_HEIGHT * 0.66, 1);

  this->ctrlBacklight(true);
}

void Display::ctrlBacklight(bool on) {
  if (on)
    digitalWrite(TFT_BL, ON);
  else
    digitalWrite(TFT_BL, OFF);
}

void Display::clearScreen() {
  tft.fillScreen(TFT_BLACK);
}

void Display::main(uint32_t currentTime) {

}