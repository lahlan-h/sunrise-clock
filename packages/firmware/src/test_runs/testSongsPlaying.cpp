#include "testSongsPlaying.h"

#include <Arduino.h>

#include "audio/Mp3Player.h"
#include "input/RotaryEncoder.h"
#include "screen/oledScreen.h"

extern Mp3Player mp3;

namespace {

constexpr uint8_t encoderButtonPin = 25;
constexpr unsigned long buttonDebounceMs = 35;
constexpr uint8_t visibleRows = 5;

OledScreen screen(21, 22, 0x3C); //sda, scl, address
RotaryEncoder encoder(32, 33);

uint8_t selectedSong = 0;
uint8_t firstVisibleSong = 0;
bool lastButtonState = HIGH;
unsigned long lastButtonChange = 0;

void drawSongList() {
  const uint8_t songCount = mp3.getTrackCount();
  const char *songNames[Mp3Player::trackCount];

  for (uint8_t i = 0; i < songCount; ++i) {
    songNames[i] = mp3.getSongName(i);
  }

  screen.drawSongList(songNames, songCount, selectedSong, firstVisibleSong);
}

void updateSelection() {
  const int32_t movement = encoder.getClicksSinceLastUpdate();
  if (movement == 0) {
    return;
  }

  const int selected = constrain(
    static_cast<int>(selectedSong) + movement,
    0,
    mp3.getTrackCount() - 1
  );
  selectedSong = static_cast<uint8_t>(selected);

  if (selectedSong < firstVisibleSong) {
    firstVisibleSong = selectedSong;
  } else if (selectedSong >= firstVisibleSong + visibleRows) {
    firstVisibleSong = selectedSong - visibleRows + 1;
  }

  drawSongList();
}

void handleButton() {
  const bool buttonState = digitalRead(encoderButtonPin);
  const unsigned long now = millis();

  if (buttonState != lastButtonState &&
      now - lastButtonChange >= buttonDebounceMs) {
    lastButtonState = buttonState;
    lastButtonChange = now;

    if (buttonState == LOW) {
      mp3.play(selectedSong + 1);
    }
  }
}

}

void testSongsPlayingBegin() {
  screen.begin();

  pinMode(encoderButtonPin, INPUT_PULLUP);
  encoder.begin();
  lastButtonState = digitalRead(encoderButtonPin);
  lastButtonChange = millis();

  drawSongList();
}

void testSongsPlayingUpdate() {
  encoder.update();
  updateSelection();
  handleButton();
}