#include "DHTSensor.h"
 
DHTSensor::DHTSensor(uint8_t dhtPin)
: dht(dhtPin, DHT11) {}

void DHTSensor::begin() {
  dht.begin();
}

void DHTSensor::update() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}

float DHTSensor::getTemperature() const {
  return temperature;
}

float DHTSensor::getHumidity() const {
  return humidity;
}

bool DHTSensor::isError() const {
  return isnan(temperature) || isnan(humidity);
}
