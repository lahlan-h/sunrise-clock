#pragma once

#include <Arduino.h>
#include <time.h>

enum class Screen { 
  Home,           // 0
  Main_Menu,      // 1
  Alarm_Setter,   // 2
  Song_Picker,    // 3
  Settings,       // 4
  Off             // 5
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
  bool buttonPressed = false;

  bool isLightOn = false;
  bool isSoundOn = true;
};   