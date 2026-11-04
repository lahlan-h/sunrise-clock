#pragma once

#include <Arduino.h>
#include <Peripheral.h>

// - - - - - - - Usage - - - - - - -
// 1  | #include "input/MotionSensor.h"
// 2  | 
// 3  | MotionSensor pir(pirPin);
// 4  | 
// 5  | void setup(){
// 6  |  pir.begin();
// 7  | } 
// 8  | 
// 9  | void loop(){
// 10 |   pir.update();
// 11 |   if(pir.motionJustStarted()){
// 12 |     Serial.println("Motion detected");
// 13 |   }
// 14 | }
// - - - - - - - - - - - - - - - - -

class MotionSensor : public Peripheral {
  public:
    MotionSensor(uint8_t pirPin);
    
    void begin() override;
    void update() override;
    void reset();

    bool motionJustStarted() const;
    bool isMotionDetected() const;

  private:
    uint8_t pirPin;
    bool wasSignalHigh = false;
    bool motionStarted = false;
};