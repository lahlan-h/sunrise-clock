#pragma once

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

class Mp3Player {
  public:
    Mp3Player(uint8_t rxPin, uint8_t txPin);
    void begin();
    void play(uint16_t track);
  private:
    uint8_t rxPin, txPin;
};





  