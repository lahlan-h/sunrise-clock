#pragma once

#include <Arduino.h>
#include "Peripheral.h"

class Mp3Player : public Peripheral{
  public:
    static constexpr uint8_t maxVolume = 30; // DFPlayer volume steps, 0 is mute

    Mp3Player(uint8_t rxPin, uint8_t txPin);

    void begin() override;
    void play(uint16_t track);
    void playSong(uint8_t track);
    void playEffect(uint8_t effect);
    void loopSong(uint8_t track);
    void pause();
    void stop();
    void setVolume(uint8_t volume);

    uint8_t getTrackCount() const;
    const char* getSongName(uint8_t trackIndex) const;
    uint8_t getEffectCount() const;
    uint8_t getVolume() const;

  private:
    static constexpr uint8_t songsFolder = 1;
    static constexpr uint8_t effectsFolder = 2;
    static constexpr uint8_t songCount = 15;
    static constexpr uint8_t effectCount = 6;
    uint8_t rxPin, txPin;
    uint8_t volume = maxVolume; // Kept here, reading it back from the DFPlayer is a slow serial query
};


//note: sound effects are:
//beep,click,eeree,pop,teleport


  