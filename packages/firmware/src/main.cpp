#include <Arduino.h>
#include "light/LightArray.h"
#include "audio/Mp3Player.h"

Mp3Player mp3(26, 27); // (rx, tx)

LightArray lights(4, 8);// (pin, #LEDs)

void setup(){
  lights.begin();
  lights.startSunrise(10000); // 10 seconds
  mp3.begin();
  mp3.play(1); // Play track 1
}

void loop(){

  




  lights.update();
}