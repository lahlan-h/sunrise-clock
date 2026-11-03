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

  char* ssid = "";
  char* ble_advertiser_name = "";

  bool isWifiConnected = false;
  bool isBluetoothConnected = false;
  bool isAlarmSet = false;
  uint8_t alarmHour = 0;
  uint8_t alarmMinute = 0;
  bool lightOn = false;
  uint8_t lightBrightnessPercent = 0;
  bool isSoundOn = true;
  bool isTimeSynced = false;
};