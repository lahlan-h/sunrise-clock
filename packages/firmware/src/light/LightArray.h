#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// - - - - - - Warning - - - - - - 
// This implementation is for the
// Freenove 8 RGB LED Module. Should
// have been included in IoT kit. Works 
// with any WS2812 strip.
// - - - - - - - - - - - - - - - -

// - - - - - - - Usage - - - - - - -
// 1  | #include "light/LightArray.h"
// 2  | 
// 3  | LightArray lights(pin, num_of_leds);
// 4  | 
// 5  | void setup(){
// 6  |  lights.begin();
// 7  |  lights.startSunrise(duration_ms); 
// 8  | }
// 9  | 
// 10 | void loop(){
// 11 |   lights.update();
// 12 | }
// - - - - - - - - - - - - - - - - -

class LightArray {
  public:
    LightArray(uint8_t pin, uint16_t count);
    
    void begin();   // Setup the LightArray for use
    void update();  // Increase the 'tick' of LightArray

    void startSunrise(unsigned long durationMs);
    void off();
    bool isRunning();

  private:
    Adafruit_NeoPixel strip;
    unsigned long startMs = 0;
    unsigned long durationMs = 0;
    bool running = false;

    uint32_t sunriseColour(float progress);
};