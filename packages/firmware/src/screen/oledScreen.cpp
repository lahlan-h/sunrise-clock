#include "OledScreen.h"

static const uint8_t ROW_HEIGHT = 11;
static const uint8_t VISIBLE_ROWS = 6;

OledScreen::OledScreen(uint8_t sdaPin, uint8_t sclPin, uint8_t i2cAddress)
: u8g2(U8G2_R0, U8X8_PIN_NONE, sclPin, sdaPin) {
  u8g2.setI2CAddress(i2cAddress << 1); // i2c addresses are 7-bits, U8G2 library expects 8-bits
}

void OledScreen::begin() {
  u8g2.begin();
}

void OledScreen::drawSongList(
  const char *const songNames[],
  uint8_t songCount,
  uint8_t selectedSong,
  uint8_t firstVisibleSong
) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  for (uint8_t row = 0; row < VISIBLE_ROWS; ++row) {
    const uint8_t songIndex = firstVisibleSong + row;
    if (songIndex >= songCount) {
      break;
    }

    const uint8_t y = row * ROW_HEIGHT;
    if (songIndex == selectedSong) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, y, 128, ROW_HEIGHT);
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }

    u8g2.drawStr(2, y + 9, songNames[songIndex]);
  }

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}