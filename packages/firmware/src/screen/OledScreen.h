#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

#include <DeviceState.h>
#include <Peripheral.h>

class OledScreen : public Peripheral {
  public:
    OledScreen(uint8_t sdaPin, uint8_t sclPin, uint8_t i2cAddress = 0x3C);

    void begin() override;

    void drawHome(const DeviceState& state);
    void drawWakeUp();
    void drawSettings();
    void drawSongList(const char* const songNames[], uint8_t songCount, uint8_t selectedSong, uint8_t firstVisibleSong);

    /* 
    void drawSongList(
      const char* const songNames[],
      uint8_t songCount,
      uint8_t selectedSong,
      uint8_t firstVisibleSong
    );
    */

  private:
    U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;
};