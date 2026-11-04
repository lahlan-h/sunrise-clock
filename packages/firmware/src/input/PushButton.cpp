#include "PushButton.h"

PushButton::PushButton(uint8_t btnPin, uint16_t btnDebounceMs, ButtonWiring wiring)
: btnPin(btnPin),
  btnDebounceMs(btnDebounceMs),
  pressedLevel(wiring == ButtonWiring::ToGround ? LOW : HIGH),
  lastReading(wiring == ButtonWiring::ToGround ? HIGH : LOW) {} // Start at the released level

void PushButton::begin(){
  // The internal resistor holds the pin at the released level, so no external resistor is needed
  pinMode(btnPin, pressedLevel == LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
}

void PushButton::update(){
  unsigned long currentMillis = millis();
  int reading = digitalRead(btnPin);

  if (reading != lastReading) {
    lastChangeMillis = currentMillis;
    lastReading = reading;
  }

  bool wasPushed = isPushed;
  if (currentMillis - lastChangeMillis >= btnDebounceMs) {
    isPushed = (reading == pressedLevel);
  }
  justPressed = isPushed && !wasPushed;
}

bool PushButton::getIsPushed() const {
  return isPushed;
}

bool PushButton::wasPressed() const {
  return justPressed;
}
