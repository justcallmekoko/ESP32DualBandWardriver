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

  this->drawLogoCentered(logo, 90, 80);

  this->ctrlBacklight(true);
}

void Display::drawLogoCentered(const uint8_t* bitmap, int w, int h) {
  const int rowSize = 12;       // Each row is padded to 4-byte boundary
  const int dataOffset = 62;    // Start of bitmap pixel data

  int x_offset = (160 - w) / 2;
  int y_offset = (80 - h) / 2;

  tft->startWrite();

  for (int row = 0; row < h; row++) {
      int y = y_offset + (h - 1 - row);  // BMP stores bottom-up

      for (int col = 0; col < w; col++) {
          int byteIndex = dataOffset + row * rowSize + (col / 8);
          uint8_t byteVal = pgm_read_byte(&bitmap[byteIndex]);
          bool pixelOn = (byteVal >> (7 - (col % 8))) & 0x01;

          uint16_t color = pixelOn ? ST77XX_BLACK : ST77XX_WHITE;
          tft->writePixel(x_offset + col, y, color);
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