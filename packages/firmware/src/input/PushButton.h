#pragma once

#include <Arduino.h>

// - - - - - - Warning - - - - - - 
// This implementation uses INPUT_PULLUP
// so that mean when button left alone
// it pulls HIGH.
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
// 12 | }
// - - - - - - - - - - - - - - - - -

class PushButton{
  public:
    PushButton(uint8_t btnPin, uint16_t btnDebounceMs);

    void begin();
    void update();
    bool getIsPushed() const;

  private:
    uint8_t btnPin;
    uint16_t btnDebounceMs; 

    uint8_t lastReading = HIGH; 
    unsigned long lastChangeMillis = 0;
    
    bool isPushed = false;
};
