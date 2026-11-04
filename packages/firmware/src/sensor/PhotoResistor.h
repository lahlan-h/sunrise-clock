#pragma once

#include <Arduino.h>
#include <Peripheral.h>

// - - - - - - - Usage - - - - - - -
// 1  | #include <Arduino.h>
// 2  | #include "input/PhotoResistor.h"
// 3  | 
// 4  | PhotoResistor ldr(ldrPin);
// 5  | 
// 6  | void setup(){ 
// 7  |  Serial.begin(115200);
// 8  |  ldr.begin();
// 9  | }
// 10 | 
// 11 | void loop(){  
// 12 |  ldr.update();
// 13 |  Serial.print("Light Level: ");
// 14 |  Serial.println(ldr.getLightLevel());
// 15 | }
// - - - - - - - - - - - - - - - - -

class PhotoResistor : public Peripheral {
  public:
    PhotoResistor(uint8_t ldrPin);

    void begin() override;
    void update() override;

    int getLightLevel();
    boolean isNight();
    void setLightLevelThreshold(int threshold);
    
  private: 
    // Set LDR Pin
    uint8_t ldrPin;

    // Light Level Value
    int lightLevel = 0;

    // Clamp range and threshold values for photoresistor
    const int minLightLevel = 0;
    const int maxLightLevel = 100;
    int lightLevelThreshold = 25;

    int readLightLevel();
};