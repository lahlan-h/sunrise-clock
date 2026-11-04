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
  int16_t wifiRssi = 0;
  int32_t rotaryPosition = 0;

  const const char* ssid = "";
  const const char* ble_advertiser_name = "";
  const char* firmwareVersion = "1.0.0";

  float temperature = NAN;
  float humidity = NAN;

  int ambientLightPercent = 0;


  bool motionDetected = false;
  bool buttonPressed = false;
  bool temperatureValid = false;
  bool humidityValid = false;
  bool isWifiConnected = false;
  bool isBluetoothConnected = false;
  bool isAlarmSet = false;
  bool isSoundOn = true;
  bool isTimeSynced = false;
};