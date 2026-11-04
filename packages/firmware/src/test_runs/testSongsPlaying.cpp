#include "testSongsPlaying.h"

#include <Arduino.h>

#include "audio/Mp3Player.h"
#include "input/PushButton.h"
#include "input/RotaryEncoder.h"
#include "light/LightArray.h"
#include "screen/oledScreen.h"
#include "sensor/DHTSensor.h"

extern Mp3Player mp3;
extern LightArray lights;
extern DHTSensor dht;

namespace {

constexpr uint8_t encoderButtonPin = 25;
constexpr uint8_t pauseButtonPin = 13;
constexpr uint16_t buttonDebounceMs = 35;
constexpr unsigned long sensorLogIntervalMillis = 1000;
constexpr uint8_t visibleRows = 5;
constexpr uint8_t maxSongs = 32;

OledScreen screen(21, 22, 0x3C); //sda, scl, address
RotaryEncoder encoder(32, 33);
PushButton button(encoderButtonPin, buttonDebounceMs);
PushButton pauseButton(pauseButtonPin, buttonDebounceMs);

uint8_t selectedSong = 0;
uint8_t firstVisibleSong = 0;
bool wasButtonPushed = false;
bool wasPauseButtonPushed = false;
unsigned long lastSensorLogMillis = 0;

void drawSongList() {
  const uint8_t songCount = mp3.getTrackCount();
  const char *songNames[maxSongs];

  for (uint8_t i = 0; i < songCount && i < maxSongs; ++i) {
    songNames[i] = mp3.getSongName(i);
  }

  screen.drawSongList(
    songNames,
    min(songCount, maxSongs),
    selectedSong,
    firstVisibleSong
  );
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

void playSelectedSong() {
  const bool isButtonPushed = button.getIsPushed();
  if (isButtonPushed && !wasButtonPushed) {
    mp3.play(selectedSong + 1);
  }

  wasButtonPushed = isButtonPushed;
}

void pauseMusic() {
  const bool isPauseButtonPushed = pauseButton.getIsPushed();
  if (isPauseButtonPushed && !wasPauseButtonPushed) {
    mp3.pause();
  }

  wasPauseButtonPushed = isPauseButtonPushed;
}

}

void testSongsPlayingBegin() {
  screen.begin();

  encoder.begin();
  button.begin();
  pauseButton.begin();

  drawSongList();
}

void testSongsPlayingUpdate() {
  encoder.update();
  updateSelection();
  button.update();
  playSelectedSong();
  pauseButton.update();
  pauseMusic();

  lights.update();

  dht.update();

  const unsigned long currentMillis = millis();
  if (currentMillis - lastSensorLogMillis >= sensorLogIntervalMillis) {
    lastSensorLogMillis = currentMillis;
    Serial.print("Temperature: ");
    Serial.print(dht.getTemperature());
    Serial.print(" °C, Humidity: ");
    Serial.print(dht.getHumidity());
    Serial.println(" %");
  }
}