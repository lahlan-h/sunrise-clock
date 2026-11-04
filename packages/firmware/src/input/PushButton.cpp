#include "PushButton.h"

PushButton::PushButton(uint8_t btnPin, uint16_t btnDebounceMs)
: btnPin(btnPin), btnDebounceMs(btnDebounceMs) {}

void PushButton::begin(){
  pinMode(btnPin, INPUT_PULLUP);
}

void PushButton::update(){
  unsigned long currentMillis = millis();
  int reading = digitalRead(btnPin);

  if (reading != lastReading) {
    lastChangeMillis = currentMillis;
    lastReading = reading;
  }

  bool wasPushedLastUpdate = isPushed;
  if (currentMillis - lastChangeMillis >= btnDebounceMs) {
    isPushed = (reading == LOW);
  }
  pressedThisUpdate = isPushed && !wasPushedLastUpdate;
}

bool PushButton::getIsPushed() const {
  return isPushed;
}

bool PushButton::wasPressed() const {
  return pressedThisUpdate;
}
