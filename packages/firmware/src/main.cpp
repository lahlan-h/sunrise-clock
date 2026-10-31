#include <Arduino.h>
#include "light/LightArray.h"
#include "audio/Mp3Player.h"
#include "test_runs/testSongsPlaying.h"
#include "input/DHTSensor.h"

Mp3Player mp3(26, 27); // (rx, tx)
LightArray lights(4, 8);// (pin, #LEDs)
DHTSensor dhtSensor(18); // (dataPin)

void setup(){
  //lights.begin();
  //mp3.begin();
  Serial.begin(9600);
  dhtSensor.begin();
  //testSongsPlayingBegin();
}

void loop(){
  //testSongsPlayingUpdate();
  delay(2000);

  if (dhtSensor.update()) {
    Serial.print("Temperature: ");
    Serial.print(dhtSensor.getTemperature());
    Serial.print(" C, Humidity: ");
    Serial.print(dhtSensor.getHumidity());
    Serial.println("%");
  } else {
    Serial.println("DHT read failed");
  }

}