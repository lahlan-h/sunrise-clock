#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "audio/Mp3Player.h"
#include "input/RotaryEncoder.h"

extern Mp3Player mp3;

namespace {

constexpr uint8_t oledSdaPin = 21;
constexpr uint8_t oledSclPin = 22;
constexpr uint8_t oledAddress = 0x3C;
constexpr uint8_t encoderButtonPin = 25;
constexpr uint8_t trackCount = 14;
constexpr uint8_t firstTrackNumber = 1;
constexpr uint8_t rowHeight = 11;
constexpr uint8_t visibleRows = 5;
constexpr unsigned long buttonDebounceMs = 35;

const char *const songNames[trackCount] = { //to mp3 player
  "Help Herself",
  "Ingydar",
  "Paranoid",
  "Going Up The Country",
  "Fuel",
  "Rhiannon",
  "Rockin' Robin",
  "Welcome To The Jungle",
  "Chickens",
  "Samsung",
  "Machine Gun",
  "Trumptets",
  "Tornado Siren",
  "Beep Beep"
};

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
RotaryEncoder encoder(32, 33);

int selectedTrack = 0;
int firstVisibleTrack = 0;
bool lastButtonState = HIGH;
unsigned long lastButtonChange = 0;

void updateSelection() {
  const int movement = encoder.getClicksSinceLastUpdate();
  if (movement == 0) {
    return;
  }

  selectedTrack = constrain(selectedTrack + movement, 0, trackCount - 1);

  if (selectedTrack < firstVisibleTrack) {
    firstVisibleTrack = selectedTrack;
  } else if (selectedTrack >= firstVisibleTrack + visibleRows) {
    firstVisibleTrack = selectedTrack - visibleRows + 1;
  }
}

void drawSongList() {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);

  for (uint8_t row = 0; row < visibleRows; ++row) {
    const int trackIndex = firstVisibleTrack + row;
    if (trackIndex >= trackCount) {
      break;
    }

    const uint8_t y = row * rowHeight;
    if (trackIndex == selectedTrack) {
      display.setDrawColor(1);
      display.drawBox(0, y, 128, rowHeight);
      display.setDrawColor(0);
    } else {
      display.setDrawColor(1);
    }

    display.drawStr(2, y + 9, songNames[trackIndex]);
  }

  display.setDrawColor(1);
  display.sendBuffer();
}

void handleButton() {
  const bool buttonState = digitalRead(encoderButtonPin);
  const unsigned long now = millis();

  if (buttonState != lastButtonState && now - lastButtonChange >= buttonDebounceMs) {
    lastButtonState = buttonState;
    lastButtonChange = now;

    if (buttonState == LOW) {
      mp3.play(firstTrackNumber + selectedTrack);
    }
  }
}

}

void oledBegin() {
  Wire.begin(oledSdaPin, oledSclPin);
  display.setI2CAddress(oledAddress << 1);
  display.begin();

  pinMode(encoderButtonPin, INPUT_PULLUP);
  encoder.begin();
  lastButtonState = digitalRead(encoderButtonPin);
  lastButtonChange = millis();

  drawSongList();
}

void oledUpdate() {
  encoder.update();
  updateSelection();
  handleButton();

  if (encoder.getClicksSinceLastUpdate() != 0) {
    drawSongList();
  }
}