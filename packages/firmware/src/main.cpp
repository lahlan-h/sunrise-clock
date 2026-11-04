#include <Arduino.h>

#include "DeviceState.h"
#include "Connections.h"
#include "Peripheral.h"
#include "secrets.h"
#include "pins.h"

#include "audio/Mp3Player.h"
#include "input/RotaryEncoder.h"
#include "input/PushButton.h"
#include "sensor/PhotoResistor.h"
#include "sensor/MotionSensor.h"
#include "sensor/DHTSensor.h"
#include "light/LightArray.h"
#include "screen/OledScreen.h"
#include "connectivity/BLEConnection.h"
#include "connectivity/WiFiConnection.h"
#include "connectivity/MQTTConnection.h"
#include "connectivity/TelemetryPublisher.h"
#include "audio/Mp3Player.h"

#include "CommandRouter.h"

static const char* FIRMWARE_VERSION = "0.0.0";

DeviceState deviceState;

// Initalise Peripherals
RotaryEncoder rotary(ROTARY_ENCODER_CLK, ROTARY_ENCODER_DT);
PushButton buttonA(BTN_A_PIN);
PhotoResistor ldr(LDR_PIN);
MotionSensor pir(PIR_PIN);
DHTSensor dht(DHT_PIN);
LightArray lights(LED_STRIP_DIN);
OledScreen oled(OLED_SDA_PIN, OLED_SCL_PIN);
Mp3Player mp3(MP3_RX_PIN, MP3_TX_PIN);

// Initalise Connections
WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);
BLEConnection ble(BLE_ADVERTISER_NAME, deviceState, mp3);
MQTTConnection mqtt(wifi, MQTT_HOST, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD, MQTT_DEVICE_ID, MQTT_USE_TLS, MQTT_ROOT_CERTIFICATE);

// Initalise Cloud Chain
TelemetryPublisher telemetry(mqtt, deviceState, MQTT_DEVICE_ID, nullptr, MQTTConnection::DEFAULT_TELEMETRY_INTERVAL_MS);
CommandRouter commandRouter(deviceState, ble, mqtt, lights);

Peripheral* peripherals[] = {&rotary, &buttonA, &ldr, &pir, &dht, &lights, &oled, &mp3} ;
Connections* connections[] = {&wifi, &ble, &mqtt};

void setup(){
  Serial.begin(115200);

  // Initialise Peripherals
  for(Peripheral* p : peripherals) p->begin();

  // Initialise Connections
  for(Connections* connection : connections) connection->begin();

  telemetry.begin();

  // Populate DeviceState struct with as much informaton as we know right now
  deviceState.ssid = WIFI_SSID;
  deviceState.isWifiConnected = wifi.isConnected();
  deviceState.isTimeSynced = wifi.getTime(deviceState.currentTime);
  deviceState.bleAdvertiserName = BLE_ADVERTISER_NAME;
}

void updateDeviceState(){
  // Connections
  deviceState.isWifiConnected = wifi.isConnected();
  deviceState.wifiRssi = wifi.isConnected() ? wifi.getRSSI() : 0;
  deviceState.isBluetoothConnected = ble.isConnected();

  // Peripherals
  deviceState.temperature = dht.getTemperature();
  deviceState.humidity = dht.getHumidity();
  deviceState.temperatureValid = dht.isTemperatureValid();
  deviceState.humidityValid = dht.isHumidityValid();
  deviceState.ambientLightPercent = ldr.getLightLevel();
  deviceState.motionDetected = pir.isMotionDetected();
  deviceState.rotaryPositionSinceLastUpdate = rotary.getClicksSinceLastUpdate();
  deviceState.rotaryPositionSinceLastReset = rotary.getClicksSinceReset();
  deviceState.buttonPressed = buttonA.getIsPushed(); // Revise button implementation
  deviceState.isLightOn = lights.isOn(); // Maybe redo lights variable name 
}

void loop(){
  // Update Peripherals and Connections
  for(Peripheral* p: peripherals) p->update();
  for(Connections* connection : connections) connection->update();
  
  commandRouter.update();
  telemetry.update();
  
  updateDeviceState();

  switch(deviceState.screen){
    case Screen::Home:
      oled.drawHome(deviceState);
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