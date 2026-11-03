#pragma once

#include <Arduino.h>
#include <DHT.h>
#include <Peripheral.h>

// - - - - - - - Usage - - - - - - -
// 1  | #include "input/DHTSensor.h"
// 2  | 
// 3  | DHTSensor dht(dhtPin);
// 4  | 
// 5  | void setup(){
// 6  |  Serial.begin(115200);
// 7  |  dht.begin();
// 8  | }
// 9  | 
// 10 | void loop(){
// 11 |  dht.update();
// 12 |  Serial.println(dht.getTemperature());
// 13 |  Serial.println(dht.getHumidity());
// 14 | }
// - - - - - - - - - - - - - - - - -

class DHTSensor : public Peripheral {
  public:
    DHTSensor(uint8_t dhtPin);

    void begin() override;
    void update() override;

    // Upstream utilisation requires checking for possible NAN
    float getTemperature() const;
    float getHumidity() const;
    
    // NAN, +Infinity, and -Infinity values may indiciate sensor fault
    bool isTemperatureValid() const;
    bool isHumidityValid() const;
    bool isError() const;

  private:
    DHT dht;

    float temperature = NAN;
    float humidity = NAN;
};