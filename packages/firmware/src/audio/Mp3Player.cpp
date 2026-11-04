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
  player.volume(volume);
}

void Mp3Player::play(uint16_t track) {
  if (track == 0 || track > songCount) {
    return;
  }

  playSong(static_cast<uint8_t>(track));
}

void Mp3Player::playSong(uint8_t track) {
  if (track == 0 || track > songCount) {
    return;
  }

  player.playFolder(songsFolder, track);
}

void Mp3Player::playEffect(uint8_t effect) {
  if (effect == 0 || effect > effectCount) {
    return;
  }

  player.playFolder(effectsFolder, effect);
}

void Mp3Player::pause() {
  player.pause();
}

void Mp3Player::setVolume(uint8_t volume) {
  if (volume > maxVolume) {
    volume = maxVolume;
  }

  this->volume = volume;
  player.volume(volume);
}

uint8_t Mp3Player::getTrackCount() const {
  return songCount;
}

const char* Mp3Player::getSongName(uint8_t trackIndex) const {
  if (trackIndex < 0 || trackIndex > songCount - 1) return nullptr;
  return songNames[trackIndex];
}

uint8_t Mp3Player::getEffectCount() const {
  return effectCount;
}

uint8_t Mp3Player::getVolume() const {
  return volume;
}
