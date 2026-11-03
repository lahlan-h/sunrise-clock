#pragma once

#include <Arduino.h>
#include <time.h>

enum class Screen { 
  Home,           // 0
  Main_Menu,      // 1
  Alarm_Setter,   // 2
  Song_Picker,    // 3
  Settings,       // 4
  Off
};

struct DeviceState {
  tm currentTime = {};
  Screen screen = Screen::Home;

  const char* ssid = "";
  const char* ble_advertiser_name = "";
  const char* firmwareVersion = "1.0.0";

  float temperatureC = NAN;
  float humidityPercent = NAN;
  int ambientLightPercent = 0;
  int16_t wifiRssi = 0;
  bool motionDetected = false;
  bool buttonPressed = false;
  int32_t rotaryPosition = 0;
  bool temperatureValid = false;
  bool humidityValid = false;
  bool isWifiConnected = false;
  bool isBluetoothConnected = false;
  bool isAlarmSet = false;
  uint8_t alarmHour = 0;
  uint8_t alarmMinute = 0;
  bool lightOn = false;
  uint8_t lightBrightnessPercent = 0;
  const char* lightMode = "OFF";
  bool isSoundOn = true;
  bool isTimeSynced = false;
};