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
  #ifndef JCMK_HOST_BOARD
    tft->initR(INITR_MINI160x80_PLUGIN);
  #else
    tft->initR(INITR_MINI160x80);
  #endif

  tft->setSPISpeed(TFT_SPI_SPEED);

  this->clearScreen();
  
  tft->setTextWrap(false);

  tft->setRotation(3);

  this->drawMonochromeImage160x80(logo2, 160, 80);

  this->ctrlBacklight(true);
}

void Display::drawCenteredText(String text, bool centerVertically) {
  tft->setRotation(3);  // Landscape
  tft->setTextSize(1);  // 6x8 per char
  tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft->setTextWrap(false);

  uint8_t charWidth = 6;
  uint8_t charHeight = 8;

  uint16_t textWidth = text.length() * charWidth;
  uint16_t textHeight = charHeight;

  uint16_t x = (TFT_WIDTH - textWidth) / 2;
  uint16_t y = centerVertically ? (TFT_HEIGHT - textHeight) / 2 : 0;

  tft->setCursor(x, y);
  tft->print(text);
}

// https://javl.github.io/image2cpp/
void Display::drawMonochromeImage160x80(const uint8_t* imageData, int width, int height) {
  tft->startWrite();

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int byteIndex = (y * (width / 8)) + (x / 8);
      uint8_t byteVal = pgm_read_byte(&imageData[byteIndex]);

      // MSB first (bit 7 is leftmost pixel)
      bool pixelOn = (byteVal >> (7 - (x % 8))) & 0x01;
      uint16_t color = pixelOn ? ST77XX_WHITE : ST77XX_BLACK;

      // Adjust for rotation 3 (landscape)
      int x_rot = x;
      int y_rot = y;

      tft->writePixel(x_rot, y_rot, color);
    }
  }

  tft->endWrite();
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