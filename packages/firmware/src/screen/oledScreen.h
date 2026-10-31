#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

class OledScreen {
  public:
    OledScreen(uint8_t sdaPin, uint8_t sclPin, uint8_t i2cAddress);

    void begin();
    void drawSongList(
      const char *const songNames[],
      uint8_t songCount,
      uint8_t selectedSong,
      uint8_t firstVisibleSong
    );

  private:
    static constexpr uint8_t rowHeight = 11;
    static constexpr uint8_t visibleRows = 6;

    U8G2_SH1106_128X64_NONAME_F_HW_I2C display;
    uint8_t sdaPin;
    uint8_t sclPin;
    uint8_t i2cAddress;
};