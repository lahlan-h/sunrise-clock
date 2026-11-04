#pragma once

#include <Arduino.h>
#include <Peripheral.h>

// - - - - - - Warning - - - - - -
// Uses INPUT_PULLUP, so wire buttons
// from the pin to GND, no resistor
// needed. GPIO 34-39 have no internal
// pull-up, so they can't be used.
// - - - - - - - - - - - - - - - -

// - - - - - - - Usage - - - - - - -
// 1  | #include "input/PushButton.h"
// 2  |
// 3  | PushButton button(btnPin, btnDebounceMs);
// 4  |
// 5  | void setup(){
// 6  |  button.begin();
// 7  | }
// 8  |
// 9  | void loop(){
// 10 |   button.update();
// 11 |   bool isButtonPushed = button.getIsPushed();
// 12 |   if(button.wasPressed()) Serial.println("Pressed");
// 13 | }
// - - - - - - - - - - - - - - - - -

class PushButton : public Peripheral {
  public:
    PushButton(uint8_t btnPin, uint16_t btnDebounceMs = 40);

    void begin() override;
    void update() override;
    bool getIsPushed() const;
    bool wasPressed() const; // Only true for the update() that detected the press, so holding doesn't repeat

  private:
    uint8_t btnPin;
    uint16_t btnDebounceMs;

    uint8_t lastReading = HIGH;
    unsigned long lastChangeMillis = 0;

    bool isPushed = false;
    bool pressedThisUpdate = false;
};
