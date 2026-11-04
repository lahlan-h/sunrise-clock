#pragma once

#include <Arduino.h>
#include <time.h>

enum class Screen { 
  Home,           // 0
  Main_Menu,      // 1
  Alarm_Setter,   // 2
  Song_Picker,    // 3
  Alarm,          // 4
  Settings,       // 5
  Off             // 6
};

enum class LightMode : uint8_t {
  Off,
  Steady,
  Sunrise
};

struct DeviceState {
  Screen screen = Screen::Home;
  const char* firmwareVersion = "1.0.0";
  unsigned long upTimeMs = 0;
  uint16_t errorCount = 0;

  tm currentTime = {};
  bool isTimeSynced = false;

  bool isAlarmSet = false;
  uint8_t alarmHour = 0;
  uint8_t alarmMinute = 0;

  const char* ssid = "";
  int16_t wifiRssi = 0;
  bool isWifiConnected = false;

  const char* bleAdvertiserName = "";
  bool isBluetoothConnected = false;

  float temperature = NAN;
  bool temperatureValid = false;
  float humidity = NAN;
  bool humidityValid = false;
  int ambientLightPercent = 0;
  bool motionDetected = false;

  int32_t rotaryPositionSinceLastUpdate = 0;
  int32_t rotaryPositionSinceLastReset = 0;
  bool rotaryButtonPressed = false;
  bool backButtonPressed = false;
  bool alarmButtonPressed = false;

  bool isLightOn = false;
  uint8_t lightBrightnessPercent = 0;
  LightMode lightMode = LightMode::Off;
  uint8_t volume = 0;
  bool isSoundOn = true;
};

// Names used for these enums in BLE / MQTT JSON
inline const char* lightModeName(LightMode mode) {
  switch (mode) {
    case LightMode::Off:     return "OFF";
    case LightMode::Steady:  return "STEADY";
    case LightMode::Sunrise: return "SUNRISE";
  }
  return "OFF";
}

inline const char* screenName(Screen screen) {
  switch (screen) {
    case Screen::Home:         return "HOME";
    case Screen::Main_Menu:    return "MAIN_MENU";
    case Screen::Alarm_Setter: return "ALARM_SETTER";
    case Screen::Song_Picker:  return "SONG_PICKER";
    case Screen::Settings:     return "SETTINGS";
    case Screen::Off:          return "OFF";
  }
  return "UNKNOWN";
}
