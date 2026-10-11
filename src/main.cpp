#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "light/LightArray.h"

LightArray lights(2, 8);

void setup() {
  lights.begin();
  lights.startSunrise(15000); // 15 seconds later do 30 minutes
}

void loop() {
  lights.update();

  if(millis() >= 20000){
    lights.off();
  }
}