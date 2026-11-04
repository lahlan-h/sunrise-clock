#include <Arduino.h>

#include "DeviceState.h"
#include "ScreenState.h"
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

#include "CommandRouter.h"

static const uint8_t BLUETOOTH_CONNECTED_EFFECT = 6; 
static const uint8_t MAIN_MENU_ITEM_COUNT = 5;

DeviceState deviceState;
ScreenState screenState;

// Initalise Peripherals
RotaryEncoder rotary(ROTARY_ENCODER_CLK, ROTARY_ENCODER_DT);
PushButton rotaryButton(BTN_ROTARY_PIN);
PushButton backButton(BTN_BACK_PIN);
PushButton alarmButton(BTN_ALARM_PIN);
PhotoResistor ldr(LDR_PIN);
MotionSensor pir(PIR_PIN);
DHTSensor dht(DHT_PIN);
LightArray lights(LED_STRIP_DIN);
OledScreen oled(OLED_SDA_PIN, OLED_SCL_PIN);
Mp3Player mp3(MP3_RX_PIN, MP3_TX_PIN);

// Commands from BLE and MQTT all go through here
CommandRouter commandRouter(deviceState, lights, mp3);

// Initalise Connections
WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);
BLEConnection ble(BLE_ADVERTISER_NAME, deviceState, commandRouter);
MQTTConnection mqtt(wifi, deviceState, commandRouter, MQTT_HOST, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD, MQTT_DEVICE_ID);

Peripheral* peripherals[] = {&rotary, &rotaryButton, &backButton, &alarmButton, &ldr, &pir, &dht, &lights, &oled, &mp3} ;
Peripheral* connections[] = {&wifi, &ble, &mqtt};

void setup(){
  Serial.begin(115200);

  // Initialise Peripherals
  for(Peripheral* p : peripherals) p->begin();

  // Initialise Connections (the router must be ready before BLE/MQTT deliver commands)
  commandRouter.begin();
  for(Peripheral* connection : connections) connection->begin();

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
  deviceState.isTimeSynced = wifi.getTime(deviceState.currentTime);
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
  deviceState.rotaryButtonPressed = rotaryButton.getIsPushed();
  deviceState.backButtonPressed = backButton.getIsPushed();
  deviceState.alarmButtonPressed = alarmButton.getIsPushed();
  deviceState.isLightOn = lights.isOn(); // Maybe redo lights variable name
  deviceState.lightBrightnessPercent = lights.getBrightnessPercent();
  deviceState.lightMode = lights.getMode();
  deviceState.volume = mp3.getVolume();

  deviceState.upTimeMs = millis();
}

void loop(){
  // Update Peripherals and Connections
  for(Peripheral* p: peripherals) p->update();
  for(Peripheral* connection : connections) connection->update();

  Serial.println(ldr.getLightLevel());

  // Apply commands queued by BLE / MQTT, then refresh the shared state
  bool commandApplied = commandRouter.update();
  bool wasBluetoothConnected = deviceState.isBluetoothConnected;
  
  updateDeviceState();

  if (deviceState.isBluetoothConnected && !wasBluetoothConnected) mp3.playEffect(BLUETOOTH_CONNECTED_EFFECT);

  // Push changes out now rather than waiting for the next scheduled refresh
  if (commandApplied) {
    ble.notifyState();
    mqtt.publishState();
  }

  switch(deviceState.screen){
    case Screen::Home:
      if(rotaryButton.wasPressed()) deviceState.screen = Screen::Main_Menu;
      if(alarmButton.wasPressed()) deviceState.isAlarmSet = !deviceState.isAlarmSet;
      oled.drawHomeScreen(deviceState);
      break;
    case Screen::Main_Menu:
      // Stretch goal: after 15s return back to Home screen  
      if(backButton.wasPressed()) deviceState.screen = Screen::Home;
      screenState.selectedMainMenuItemIndex = ((screenState.selectedMainMenuItemIndex + rotary.getClicksSinceLastUpdate()) % MAIN_MENU_ITEM_COUNT + MAIN_MENU_ITEM_COUNT) % MAIN_MENU_ITEM_COUNT;
      
      if(screenState.selectedMainMenuItemIndex == 0 && rotaryButton.wasPressed()) deviceState.screen = Screen::Alarm_Setter;
      if(screenState.selectedMainMenuItemIndex == 1 && rotaryButton.wasPressed()) deviceState.screen = Screen::Song_Picker;
      if(screenState.selectedMainMenuItemIndex == 2 && rotaryButton.wasPressed()) deviceState.screen = Screen::Brightness_Setter;
      if(screenState.selectedMainMenuItemIndex == 3 && rotaryButton.wasPressed()) deviceState.screen = Screen::Volume_Setter;
      if(screenState.selectedMainMenuItemIndex == 4 && rotaryButton.wasPressed()) deviceState.screen = Screen::Info;

      oled.drawMainMenuScreen(deviceState, screenState);
      break;
    case Screen::Alarm_Setter:
      break;
    case Screen::Song_Picker:
      break;
    case Screen::Brightness_Setter:
      break;
    case Screen::Volume_Setter:
      break;
    case Screen::Info:
      if(backButton.wasPressed()) deviceState.screen = Screen::Home;
      oled.drawInfoScreen(deviceState);
      break;
    case Screen::Alarm:
      break;
    case Screen::Off:
    oled.drawOffScreen();  
    break;
  }
}
