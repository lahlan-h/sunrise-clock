#pragma once

#include <Arduino.h>
#include <Peripheral.h>

// - - - - - - Warning - - - - - -
// Uses the ESP32's internal pull-up
// (or pull-down) resistor, so buttons
// don't need their own. GPIO 34-39
// have no internal resistors, so
// don't put buttons on those pins.
// - - - - - - - - - - - - - - - -

// - - - - - - - Usage - - - - - - -
// 1  | #include "input/PushButton.h"
// 2  |
// 3  | PushButton button(btnPin); // Button wired from pin to GND
// 4  | // PushButton button(btnPin, 40, ButtonWiring::To3V3); // Wired from pin to 3.3V
// 5  |
// 6  | void setup(){
// 7  |  button.begin();
// 8  | }
// 9  |
// 10 | void loop(){
// 11 |   button.update();
// 12 |   bool isButtonPushed = button.getIsPushed(); // Held down right now
// 13 |   if(button.wasPressed()) Serial.println("Pressed"); // Once per press
// 14 | }
// - - - - - - - - - - - - - - - - -

// Where the button's other leg is wired
enum class ButtonWiring : uint8_t {
  ToGround, // Internal pull-up, reads LOW while pressed
  To3V3     // Internal pull-down, reads HIGH while pressed
};

class PushButton : public Peripheral {
  public:
    PushButton(uint8_t btnPin, uint16_t btnDebounceMs = 40, ButtonWiring wiring = ButtonWiring::ToGround);

    void begin() override;
    void update() override;
    bool getIsPushed() const; // True while the button is held down
    bool wasPressed() const;  // True for one update() after each press

  private:
    uint8_t btnPin;
    uint16_t btnDebounceMs;
    uint8_t pressedLevel;

    uint8_t lastReading;
    unsigned long lastChangeMillis = 0;

    bool isPushed = false;
    bool justPressed = false;
};
