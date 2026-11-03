#include "PhotoResistor.h"

// ADC behaviour
static const uint8_t LDR_ADC_RESOLUTION = 12; // 10 bits = 0 to 1023, 12 bits = 0 to 4095
static const uint16_t ADC_FULL_SCALE = (1 << LDR_ADC_RESOLUTION) - 1;

// LDR behaviour
static const uint16_t LDR_ADC_MIN = static_cast<uint16_t>(0.1 * ADC_FULL_SCALE);
static const uint16_t LDR_ADC_MAX = static_cast<uint16_t>(0.8 * ADC_FULL_SCALE);

PhotoResistor::PhotoResistor(uint8_t ldrPin)
: ldrPin(ldrPin) {}

void PhotoResistor::begin(){
  pinMode(ldrPin, INPUT);
  analogReadResolution(LDR_ADC_RESOLUTION);
}

void PhotoResistor::update(){
  lightLevel = readLightLevel();
}

int PhotoResistor::getLightLevel(){
  return lightLevel;
}

int PhotoResistor::readLightLevel(){
  int adcValue = analogRead(ldrPin);
  int lightLevel = map(constrain(adcValue, LDR_ADC_MIN, LDR_ADC_MAX), LDR_ADC_MIN, LDR_ADC_MAX, minLightLevel, maxLightLevel);
  return lightLevel;
}


void PhotoResistor::setLightLevelThreshold(int threshold){
  if(threshold < minLightLevel || minLightLevel > maxLightLevel) return;
  lightLevelThreshold = threshold;
}

boolean PhotoResistor::isNight(){
  return lightLevel <= lightLevelThreshold;
}