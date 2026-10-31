#include <Arduino.h>
#include "light/LightArray.h"
#include "audio/Mp3Player.h"

void oledBegin();
void oledUpdate();

Mp3Player mp3(26, 27); // (rx, tx)

LightArray lights(4, 8);// (pin, #LEDs)

void setup(){
  lights.begin();
  mp3.begin();
  oledBegin();
}

void loop(){
  oledUpdate();
}