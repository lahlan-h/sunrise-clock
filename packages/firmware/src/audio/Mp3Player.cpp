#include "Mp3Player.h"

#include <DFRobotDFPlayerMini.h>

static HardwareSerial mp3Serial(2);
static DFRobotDFPlayerMini player;

static const char* songNames[15] = {
  "Help Herself",
  "Ingydar",
  "Paranoid",
  "Going Up The Country",
  "Fuel",
  "Rhiannon",
  "Rockin' Robin",
  "Welcome To The Jungle",
  "Guten Morgen Sonnenschein",
  "Samsung",
  "Machine Gun",
  "Trumptets",
  "Tornado Siren",
  "Beep Beep",
  "Chickens"
};

Mp3Player::Mp3Player(uint8_t rxPin, uint8_t txPin) 
: rxPin(rxPin), txPin(txPin) {}

void Mp3Player::begin() {
  mp3Serial.begin(9600, SERIAL_8N1, rxPin, txPin);
  player.begin(mp3Serial);
  player.volume(30);
}

void Mp3Player::play(uint16_t track) {
  player.play(track);
}

uint8_t Mp3Player::getTrackCount() const {
  return trackCount;
}

const char* Mp3Player::getSongName(uint8_t trackIndex) const {
  if (trackIndex >= trackCount) {
    return "";
  }

  return songNames[trackIndex];
}
