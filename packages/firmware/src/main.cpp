#include <Arduino.h>

#include "DeviceState.h"
#include "Peripheral.h"
#include "pins.h"
#include "secrets.h"

#include "input/RotaryEncoder.h"
#include "input/PushButton.h"
#include "sensor/PhotoResistor.h"
#include "sensor/MotionSensor.h"
#include "sensor/DHTSensor.h"
#include "light/LightArray.h"
#include "screen/OledScreen.h"
#include "connectivity/WiFiConnection.h"

RotaryEncoder rotary(ROTARY_ENCODER_CLK, ROTARY_ENCODER_DT);
PushButton buttonA(BTN_A_PIN);
PhotoResistor ldr(LDR_PIN);
MotionSensor pir(PIR_PIN);
DHTSensor dht(DHT_PIN);
LightArray lights(LED_STRIP_DIN);
OledScreen oled(OLED_SDA_PIN, OLED_SCL_PIN);
WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);

Peripheral* peripherals[] = {&rotary, &buttonA, &ldr, &pir, &dht, &lights, &oled } ;
DeviceState deviceState;

void setup(){
  Serial.begin(115200);

  // Initialise Peripherals
  for(Peripheral* p : peripherals) p->begin();

  // Initialise Connectivity
  wifi.begin();

  // Populate DeviceState struct with as much informaton as we know right now
  deviceState.ssid = const_cast<char *>(WIFI_SSID);
  deviceState.isWifiConnected = wifi.isConnected();
  deviceState.isTimeSynced = wifi.getTime(deviceState.currentTime);
}

void loop(){
  // Update Peripherals
  for(Peripheral* p: peripherals) p->update();
  
  switch(deviceState.screen){
    case Screen::Home:
      break;
    case Screen::Main_Menu:
      break;
    case Screen::Alarm_Setter:
      break;
    case Screen::Song_Picker:
      break;
    case Screen::Settings:
      break;
    case Screen::Off:
      break;
  }
}