#pragma once

#include <Arduino.h>

// - - - - - - Warning - - - - - - 
// Written for a  KY-040 module. Power
// it from 3.3V, as it's pull-ups put
// the + pin voltage onto CLK and DT 
// - - - - - - - - - - - - - - - -

// - - - - - - - Usage - - - - - - -
// 1  | #include "input/RotaryEncoder.h"
// 2  | 
// 3  | RotaryEncoder rotary(clkPin, dtPin);
// 4  | 
// 5  | void setup(){
// 6  |  rotary.begin();
// 7  | } 
// 8  | 
// 9  | void loop(){
// 10 |   rotary.update();
// 11 |   int32_t clicksTurned = rotary.getClicksSinceLastUpdate();
// 12 | }
// - - - - - - - - - - - - - - - - -

class RotaryEncoder {
  public:
    RotaryEncoder(uint8_t clkPin, uint8_t dtPin);

    void begin();         
    void update();  // Getters change when this runs, update once per loop
    
    void reset();
    int32_t getClicksSinceReset();
    int32_t getClicksSinceLastUpdate(); 
    
  private:
    uint8_t clkPin, dtPin;

    // Only the interrupt touches these after begin
    uint8_t previousPinState = 0;
    int32_t pendingTransitions = 0;

    // Shared with the interrupt, so it's volatile
    volatile int32_t unreadClicks = 0;

    int32_t clicksSinceReset = 0;
    int32_t clicksSinceLastUpdate = 0;

    uint8_t readPinState();
};