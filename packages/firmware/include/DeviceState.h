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
  tm currentTime = {};
  Screen screen = Screen::Home;

  const char* firmware_version = "0.0.0";
  unsigned long uptime = 0;
  uint16_t errors = 0;
  
  const char* ssid = "";
  const char* ble_advertiser_name = "";

  bool isWifiConnected = false;
  bool isBluetoothConnected = false;
  bool isAlarmSet = true;
  bool isSoundOn = true;
  bool isTimeSynced = false;

  float temperature = NAN;
  float humidity = NAN;
};