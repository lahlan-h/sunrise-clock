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

static const uint8_t BRIGHTNESS_STEP_PERCENT = 5;
static const uint8_t BLUETOOTH_CONNECTED_EFFECT = 6; 
static const uint8_t ROTARY_PRESS_EFFECT = 4;
static const uint8_t ROTARY_TURN_EFFECT = 2;
static const uint8_t BACK_PRESS_EFFECT = 5;
static const uint8_t MAIN_MENU_ITEM_COUNT = 5;
static const uint8_t HOURS_PER_DAY = 24;
static const uint8_t MINUTES_PER_HOUR = 60;
static const uint32_t SECONDS_PER_DAY = 24UL * 60UL * 60UL;
static const uint32_t SUNRISE_DURATION_S = 30UL * 60UL;
static const unsigned long ALARM_RING_LIMIT_MS = 5UL * 60UL * 1000UL;
static const uint8_t NIGHT_ENTER_LIGHT_PERCENT = 10;
static const uint8_t NIGHT_EXIT_LIGHT_PERCENT = 15; // Higher than enter, so it doesn't flicker at dusk
static const unsigned long SCREEN_WAKE_MS = 15000;

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

bool anyButtonWasPressed(){
  return rotaryButton.wasPressed() || backButton.wasPressed() || alarmButton.wasPressed();
}

// Starts the sunrise once the alarm is within SUNRISE_DURATION_S, so the light peaks at the alarm time
void startAlarmIfDue(){
  if(!deviceState.isAlarmSet || !deviceState.isTimeSynced) return;

  uint32_t secondsNow = deviceState.currentTime.tm_hour * 3600UL + deviceState.currentTime.tm_min * 60UL + deviceState.currentTime.tm_sec;
  uint32_t secondsAlarm = deviceState.alarmHour * 3600UL + deviceState.alarmMinute * 60UL;
  uint32_t secondsUntilAlarm = (secondsAlarm + SECONDS_PER_DAY - secondsNow) % SECONDS_PER_DAY;

  // Re-arm outside the window, so it rings again tomorrow
  if(secondsUntilAlarm > SUNRISE_DURATION_S) {
    screenState.isAlarmWindowHandled = false;
    return;
  }
  if(screenState.isAlarmWindowHandled) return;

  // Set less than 30 min ahead, the sunrise just runs shorter
  if(secondsUntilAlarm == 0) secondsUntilAlarm = 1; // Avoids dividing by a zero duration

  screenState.isAlarmWindowHandled = true;
  screenState.isAlarmRinging = false;
  screenState.sunriseStartMs = millis();
  screenState.sunriseDurationMs = secondsUntilAlarm * 1000UL;
  screenState.alarmPhraseIndex = random(OledScreen::alarmPhraseCount);
  lights.startSunrise(screenState.sunriseDurationMs);
  deviceState.screen = Screen::Alarm;
}

// Dark room turns the screen off once idle, motion or a press wakes it (handled in Screen::Off)
void updateNightMode(){
  if(deviceState.ambientLightPercent < NIGHT_ENTER_LIGHT_PERCENT) screenState.isNightMode = true;
  if(deviceState.ambientLightPercent > NIGHT_EXIT_LIGHT_PERCENT) screenState.isNightMode = false;

  if(deviceState.motionDetected || deviceState.rotaryButtonPressed || deviceState.backButtonPressed || deviceState.alarmButtonPressed || rotary.getClicksSinceLastUpdate() != 0) {
    screenState.lastActivityMs = millis();
  }

  if(!screenState.isNightMode) {
    if(deviceState.screen == Screen::Off) deviceState.screen = Screen::Home;
    return;
  }

  if(deviceState.screen == Screen::Alarm || deviceState.screen == Screen::Off) return;
  if(millis() - screenState.lastActivityMs >= SCREEN_WAKE_MS) deviceState.screen = Screen::Off;
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

  // Before the screen switch, so a screen that plays a song (e.g. the song picker) overrides the effect
  if (rotaryButton.wasPressed()) mp3.playEffect(ROTARY_PRESS_EFFECT);
  if (rotary.getClicksSinceLastUpdate() != 0) mp3.playEffect(ROTARY_TURN_EFFECT);
  if (backButton.wasPressed()) mp3.playEffect(BACK_PRESS_EFFECT);

  // Push changes out now rather than waiting for the next scheduled refresh
  if (commandApplied) {
    ble.notifyState();
    mqtt.publishState();
  }

  startAlarmIfDue();
  updateNightMode();

  switch(deviceState.screen){
    case Screen::Home:
      if(rotaryButton.wasPressed()) deviceState.screen = Screen::Main_Menu;
      if(alarmButton.wasPressed()) deviceState.isAlarmSet = !deviceState.isAlarmSet;
      oled.drawHomeScreen(deviceState);
      break;
    case Screen::Main_Menu:
      if(backButton.wasPressed()) deviceState.screen = Screen::Home;
      screenState.selectedMainMenuItemIndex = ((screenState.selectedMainMenuItemIndex + rotary.getClicksSinceLastUpdate()) % MAIN_MENU_ITEM_COUNT + MAIN_MENU_ITEM_COUNT) % MAIN_MENU_ITEM_COUNT;
      
      if(screenState.selectedMainMenuItemIndex == 0 && rotaryButton.wasPressed()) {
        screenState.alarmHour = deviceState.alarmHour;
        screenState.alarmMinute = deviceState.alarmMinute;
        screenState.isEditingAlarmMinute = false;
        deviceState.screen = Screen::Alarm_Setter;
      }
      if(screenState.selectedMainMenuItemIndex == 1 && rotaryButton.wasPressed()) {
        screenState.selectedSongIndex = deviceState.alarmSongIndex;
        deviceState.screen = Screen::Song_Picker;
      }
      if(screenState.selectedMainMenuItemIndex == 2 && rotaryButton.wasPressed()) {
        lights.applyControl(screenState.brightnessPercent, LightMode::Steady); 
        deviceState.screen = Screen::Brightness_Setter;
      } 
      if(screenState.selectedMainMenuItemIndex == 3 && rotaryButton.wasPressed()) {
        lights.showLevelMeter(mp3.getVolume(), Mp3Player::maxVolume);
        deviceState.screen = Screen::Volume_Setter;
      }
      if(screenState.selectedMainMenuItemIndex == 4 && rotaryButton.wasPressed()) deviceState.screen = Screen::Info;

      oled.drawMainMenuScreen(deviceState, screenState);
      break;
    case Screen::Alarm_Setter:
      if(backButton.wasPressed()) deviceState.screen = Screen::Main_Menu;
      if(screenState.isEditingAlarmMinute) screenState.alarmMinute = ((screenState.alarmMinute + rotary.getClicksSinceLastUpdate()) % MINUTES_PER_HOUR + MINUTES_PER_HOUR) % MINUTES_PER_HOUR;
      else screenState.alarmHour = ((screenState.alarmHour + rotary.getClicksSinceLastUpdate()) % HOURS_PER_DAY + HOURS_PER_DAY) % HOURS_PER_DAY;

      if(rotaryButton.wasPressed() && !screenState.isEditingAlarmMinute) screenState.isEditingAlarmMinute = true;
      else if(rotaryButton.wasPressed()) {
        deviceState.alarmHour = screenState.alarmHour;
        deviceState.alarmMinute = screenState.alarmMinute;
        deviceState.screen = Screen::Home;
        ble.notifyState();
        mqtt.publishState();
      }
      oled.drawAlarmSetterScreen(deviceState, screenState);
      break;
    case Screen::Song_Picker:
      if(backButton.wasPressed()) {
        mp3.pause();
        deviceState.screen = Screen::Main_Menu;
      }
      screenState.selectedSongIndex = constrain(screenState.selectedSongIndex + rotary.getClicksSinceLastUpdate(), 0, mp3.getTrackCount() - 1);
      if(rotaryButton.wasPressed()) {
        deviceState.alarmSongIndex = screenState.selectedSongIndex;
        mp3.playSong(screenState.selectedSongIndex + 1); // DFPlayer tracks start at 1
      }
      oled.drawSongPickerScreen(deviceState, screenState, mp3);
      break;
    case Screen::Brightness_Setter:
      if(rotary.getClicksSinceLastUpdate() != 0){
        screenState.brightnessPercent = constrain(screenState.brightnessPercent + rotary.getClicksSinceLastUpdate() * BRIGHTNESS_STEP_PERCENT, 0, 100);
        lights.applyControl(screenState.brightnessPercent, screenState.brightnessPercent > 0 ? LightMode::Steady : LightMode::Off);
      }
      if(rotaryButton.wasPressed() || backButton.wasPressed()) {
        lights.off();
        deviceState.screen = Screen::Main_Menu;
      }
      oled.drawBrightnessSetterScreen(deviceState, screenState);
      break;
    case Screen::Volume_Setter:
      if(rotary.getClicksSinceLastUpdate() != 0){
        mp3.setVolume(constrain(mp3.getVolume() + rotary.getClicksSinceLastUpdate(), 0, Mp3Player::maxVolume));
        lights.showLevelMeter(mp3.getVolume(), Mp3Player::maxVolume);
      }
      if(rotaryButton.wasPressed() || backButton.wasPressed()) {
        lights.off();
        deviceState.screen = Screen::Main_Menu;
        ble.notifyState();
        mqtt.publishState();
      }
      oled.drawVolumeSetterScreen(deviceState);
      break;
    case Screen::Info:
      if(backButton.wasPressed()) deviceState.screen = Screen::Home;
      oled.drawInfoScreen(deviceState);
      break;
    case Screen::Alarm:
      if(!screenState.isAlarmRinging && millis() - screenState.sunriseStartMs >= screenState.sunriseDurationMs) {
        screenState.isAlarmRinging = true;
        screenState.alarmRingingStartMs = millis();
        mp3.loopSong(deviceState.alarmSongIndex + 1); // DFPlayer tracks start at 1
      }
      // Any button stops it, during the sunrise too, as does the alarm being turned off (e.g. from the app)
      if(!deviceState.isAlarmSet || anyButtonWasPressed() || (screenState.isAlarmRinging && millis() - screenState.alarmRingingStartMs >= ALARM_RING_LIMIT_MS)) {
        mp3.stop();
        lights.off();
        screenState.isAlarmRinging = false;
        deviceState.screen = Screen::Home;
      }
      oled.drawAlarmScreen(deviceState, screenState);
      break;
    case Screen::Off:
      // The waking press only wakes, it doesn't also act on Home
      if(deviceState.motionDetected || anyButtonWasPressed() || rotary.getClicksSinceLastUpdate() != 0) deviceState.screen = Screen::Home;
      oled.drawOffScreen();
      break;
  }
}
