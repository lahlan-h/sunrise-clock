#pragma once

#include <Arduino.h>

struct ScreenState {
  // Menu State
  uint8_t selectedMainMenuItemIndex = 0;

  // Brightness State
  uint8_t brightnessPercent = 50;

  // Alarm Setter State (a draft, only saved to DeviceState on confirm)
  uint8_t alarmHour = 0;
  uint8_t alarmMinute = 0;
  bool isEditingAlarmMinute = false;

  // Song Picker State
  uint8_t selectedSongIndex = 0;

  // Alarm State
  bool isAlarmWindowHandled = false; // Stops a dismissed alarm restarting within the same sunrise window
  unsigned long sunriseStartMs = 0;
  unsigned long sunriseDurationMs = 0;
  bool isAlarmRinging = false;
  unsigned long alarmRingingStartMs = 0;
  uint8_t alarmPhraseIndex = 0;

  // Night Mode State
  bool isNightMode = false;
  unsigned long lastActivityMs = 0; // Last motion, button press or knob turn
};