#pragma once

#include <Arduino.h>
#include <DHT.h>

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

class DHTSensor {
  public:
    DHTSensor(uint8_t dhtPin);

    void begin();
    void update();

    // Upstream utilisation requires checking for possible NAN
    float getTemperature() const;
    float getHumidity() const;
    
    // NAN values may indiciate sensor fault
    bool isError() const;

  private:
    DHT dht;

    float temperature = NAN;
    float humidity = NAN;
};