#pragma once

#include <Arduino.h>
#include "Peripheral.h"

class Mp3Player : public Peripheral{
  public:
    Mp3Player(uint8_t rxPin, uint8_t txPin);

    void begin() override;
    void play(uint16_t track);
    
    uint8_t getTrackCount() const;
    const char* getSongName(uint8_t trackIndex) const;

  private:
    const uint8_t trackCount = 15;
    uint8_t rxPin, txPin;
};





  