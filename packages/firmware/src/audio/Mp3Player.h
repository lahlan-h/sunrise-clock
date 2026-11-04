#pragma once

#include <Arduino.h>
#include "Peripheral.h"

class Mp3Player : public Peripheral{
  public:
    Mp3Player(uint8_t rxPin, uint8_t txPin);

    void begin() override;
    void play(uint16_t track);
    void playSong(uint8_t track);
    void playEffect(uint8_t effect);
    void pause();
    
    uint8_t getTrackCount() const;
    const char* getSongName(uint8_t trackIndex) const;
    uint8_t getEffectCount() const;

  private:
    static constexpr uint8_t songsFolder = 1;
    static constexpr uint8_t effectsFolder = 2;
    static constexpr uint8_t songCount = 15;
    static constexpr uint8_t effectCount = 6;
    uint8_t rxPin, txPin;
};


//note: sound effects are:
//beep,click,eeree,pop,teleport


  