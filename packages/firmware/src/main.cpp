#include <Arduino.h>
#include "light/LightArray.h"

LightArray lights(4, 8);

void setup(){
  lights.begin();
}

void loop(){

  




  lights.update();
}