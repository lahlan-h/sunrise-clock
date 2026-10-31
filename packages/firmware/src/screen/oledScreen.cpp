#include "oledScreen.h"

#include <Wire.h>

OledScreen::OledScreen(uint8_t sdaPin, uint8_t sclPin, uint8_t i2cAddress)
: display(U8G2_R0, U8X8_PIN_NONE),
  sdaPin(sdaPin),
  sclPin(sclPin),
  i2cAddress(i2cAddress) {}

void OledScreen::begin() {
  Wire.begin(sdaPin, sclPin);
  display.setI2CAddress(i2cAddress << 1);
  display.begin();
}

void OledScreen::drawSongList(
  const char *const songNames[],
  uint8_t songCount,
  uint8_t selectedSong,
  uint8_t firstVisibleSong
) {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);

  for (uint8_t row = 0; row < visibleRows; ++row) {
    const uint8_t songIndex = firstVisibleSong + row;
    if (songIndex >= songCount) {
      break;
    }

    const uint8_t y = row * rowHeight;
    if (songIndex == selectedSong) {
      display.setDrawColor(1);
      display.drawBox(0, y, 128, rowHeight);
      display.setDrawColor(0);
    } else {
      display.setDrawColor(1);
    }

    display.drawStr(2, y + 9, songNames[songIndex]);
  }

  display.setDrawColor(1);
  display.sendBuffer();
}