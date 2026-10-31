#include "DHTSensor.h"

namespace {
constexpr unsigned long READ_INTERVAL_MILLIS = 60UL * 1000UL;
}

DHTSensor::DHTSensor(uint8_t dataPin)
    : dht(dataPin, DHT11) {}

void DHTSensor::begin() {
  dht.begin();
}

bool DHTSensor::update() {
  const unsigned long currentMillis = millis();
  if (hasRead &&
      currentMillis - lastReadMillis < READ_INTERVAL_MILLIS) {
    return hasValidReading;
  }

  lastReadMillis = currentMillis;
  hasRead = true;

  const float newTemperature = dht.readTemperature();
  const float newHumidity = dht.readHumidity();

  if (isnan(newTemperature) || isnan(newHumidity)) {
    return hasValidReading;
  }

  temperature = newTemperature;
  humidity = newHumidity;
  hasValidReading = true;
  return true;
}

float DHTSensor::getTemperature() const {
  return temperature;
}

float DHTSensor::getHumidity() const {
  return humidity;
}