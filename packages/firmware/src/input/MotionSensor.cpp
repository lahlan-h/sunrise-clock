#include "MotionSensor.h"

MotionSensor::MotionSensor(uint8_t pirPin)
: pirPin(pirPin) {}

void MotionSensor::begin(){
  pinMode(pirPin, INPUT);
  wasSignalHigh = digitalRead(pirPin) == HIGH; // If PIR already HIGH on start don't count as new motion
}

void MotionSensor::update(){
  bool isSignalHigh = digitalRead(pirPin) == HIGH;
  motionStarted = isSignalHigh && !wasSignalHigh;
  wasSignalHigh = isSignalHigh;
}

bool MotionSensor::motionJustStarted() const {
  return motionStarted;
}