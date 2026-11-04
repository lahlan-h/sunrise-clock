#pragma once

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <Peripheral.h>
#include "audio/Mp3Player.h"

extern Mp3Player mp3;

// - - - - - - Warning - - - - - - 
// Written for a KY-040 module. Power
// it from 3.3V, as it's pull-ups put
// the + pin voltage onto CLK and DT.
// The ESP32's hardware pulse counter
// does the counting, so clicks aren't
// missed when the loop is slow
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

class RotaryEncoder : public Peripheral {
  public:
    RotaryEncoder(uint8_t clkPin, uint8_t dtPin);

    void begin() override;         
    void update() override;  // Getters change when this runs, update once per loop
    
    void reset();
    int32_t getClicksSinceReset();
    int32_t getClicksSinceLastUpdate(); 
    
  private:
    uint8_t clkPin, dtPin;
    ESP32Encoder pulseCounter;

    int32_t clicksSinceReset = 0;
    int32_t clicksSinceLastUpdate = 0;
};
