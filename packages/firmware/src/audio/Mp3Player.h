#pragma once

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

class Mp3Player {
  public:
    static constexpr uint8_t trackCount = 15;

    Mp3Player(uint8_t rxPin, uint8_t txPin);
    void begin();
    void play(uint16_t track);
    uint8_t getTrackCount() const;
    const char* getSongName(uint8_t trackIndex) const;

  private:
    uint8_t rxPin, txPin;
};





  