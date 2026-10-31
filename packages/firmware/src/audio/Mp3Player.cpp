#include "Mp3Player.h"

static HardwareSerial mp3Serial(2);
static DFRobotDFPlayerMini player;

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
