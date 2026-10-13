#include <Arduino.h>
#include "light/LightArray.h"
#include "input/Rotary.h"

LightArray lights(4, 8);
RotaryEncode rotary(4, 5);

void setup(){
  lights.begin();
}

void loop(){

  // core logic




  lights.update();
}