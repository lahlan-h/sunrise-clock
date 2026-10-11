#pragma once

#include <Adafruit_NeoPixel.h>

// - - - - - - Warning - - - - - - 
// This implementation is for the
// Freenove 8 RGB LED Module. Should
// have been included in IoT kit.
// - - - - - - - - - - - - - - - -

class LightArray {
  public:
    LightArray(uint8_t pin, uint16_t count);

    void begin(); 
    void update();

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