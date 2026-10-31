#pragma once

#include <Arduino.h>
#include <DHT.h>

class DHTSensor {
  public:
    explicit DHTSensor(uint8_t dataPin);

    void begin();
    bool update();

    float getTemperature() const;
    float getHumidity() const;

  private:
    DHT dht;
    float temperature = NAN;
    float humidity = NAN;
    unsigned long lastReadMillis = 0;
    bool hasRead = false;
    bool hasValidReading = false;
};