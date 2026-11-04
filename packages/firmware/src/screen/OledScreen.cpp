#include "OledScreen.h"
#include "icons.h"

static const uint8_t ROW_HEIGHT = 11;
static const uint8_t VISIBLE_ROWS = 5; // 5 x 11 px rows fit in 64 px

// Alarm screen sun, which rises from behind the horizon line
static const uint8_t SUN_X = 106;
static const uint8_t SUN_RADIUS = 8;
static const uint8_t HORIZON_Y = 42;
static const uint8_t RAY_GAP = 3;
static const uint8_t RAY_LENGTH = 3;

// Kept to 24 characters, so they stay clear of the speaker icon
static const char* const alarmPhrases[OledScreen::alarmPhraseCount] = {
  "Rise and shine!",
  "Good morning, sunshine!",
  "Good morning, Vietnam!",
  "Coffee first. Then life.",
  "Seize the day!",
  "Up and at 'em!",
  "The sun is up. Are you?",
  "Time to make the donuts",
  "Hello, beautiful human!",
  "Wakey wakey!"
};

OledScreen::OledScreen(uint8_t sdaPin, uint8_t sclPin, uint8_t i2cAddress)
: u8g2(U8G2_R0, U8X8_PIN_NONE, sclPin, sdaPin) {
  u8g2.setI2CAddress(i2cAddress << 1); // i2c addresses are 7-bits, U8G2 library expects 8-bits
}

void OledScreen::begin() {
  u8g2.begin();
}

void OledScreen::drawHomeScreen(const DeviceState& deviceState) {
  char timeText[6] = "--:--";
  char periodText[3] = "";
  if (deviceState.isTimeSynced) {
    strftime(timeText, sizeof(timeText), "%I:%M", &deviceState.currentTime);
    strftime(periodText, sizeof(periodText), "%p", &deviceState.currentTime);
  }

  char temperatureText[8] = "--°C";
  char humidityText[6] = "--%";
  
  if (!isnan(deviceState.temperature)) {
    snprintf(temperatureText, sizeof(temperatureText), "%.0f°C", deviceState.temperature);
  }
  
  if (!isnan(deviceState.humidity)) {
    snprintf(humidityText, sizeof(humidityText), "%.0f%%", deviceState.humidity);
  }

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setDrawColor(1);
  u8g2.drawXBMP(9, 39, 19, 16, deviceState.isWifiConnected ? image_wifi_full_bits : image_wifi_not_connected_bits);
  u8g2.setFont(u8g2_font_timR18_tr);
  u8g2.drawStr(8, 25, timeText);
  u8g2.drawFrame(3, 4, 76, 26);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(67, 20, periodText);
  u8g2.drawFrame(83, 4, 44, 26);
  u8g2.drawXBMP(41, 39, 14, 16, deviceState.isBluetoothConnected ? image_bluetooth_bits : image_bluetooth_not_connected_bits);
  u8g2.drawXBMP(68, 39, 20, 16, deviceState.isSoundOn ? image_volume_loud_bits : image_volume_muted_bits);
  if (deviceState.isAlarmSet) u8g2.drawXBMP(105, 39, 15, 16, image_clock_alarm_bits);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(96, 26, humidityText);
  u8g2.drawUTF8(94, 15, temperatureText);
  u8g2.sendBuffer();
}

void OledScreen::drawMainMenuScreen(const DeviceState& deviceState, const ScreenState& screenState){
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x8_tr);
  screenState.selectedMainMenuItemIndex == 0 ? u8g2.drawFilledEllipse(13, 13, 4, 4) : u8g2.drawEllipse(13, 13, 4, 4);
  u8g2.drawStr(21, 16, "Set Alarm");
  screenState.selectedMainMenuItemIndex == 1 ? u8g2.drawFilledEllipse(13, 31, 4, 4) : u8g2.drawEllipse(13, 31, 4, 4);
  u8g2.drawStr(21, 35, "Set Song");
  screenState.selectedMainMenuItemIndex == 2 ? u8g2.drawFilledEllipse(13, 48, 4, 4) : u8g2.drawEllipse(13, 48, 4, 4);
  u8g2.drawStr(21, 52, "Brightness");
  screenState.selectedMainMenuItemIndex == 3 ? u8g2.drawFilledEllipse(81, 13, 4, 4) : u8g2.drawEllipse(81, 13, 4, 4);
  u8g2.drawStr(91, 16, "Volume");
  screenState.selectedMainMenuItemIndex == 4 ? u8g2.drawFilledEllipse(81, 31, 4, 4) : u8g2.drawEllipse(81, 31, 4, 4);
  u8g2.drawStr(91, 35, "Info");
  u8g2.sendBuffer();
}

void OledScreen::drawSongPickerScreen(const DeviceState& deviceState, const ScreenState& screenState, const Mp3Player& mp3) {
  // Scroll just far enough to keep the selected song on screen
  uint8_t firstVisibleSongIndex = screenState.selectedSongIndex < VISIBLE_ROWS ? 0 : screenState.selectedSongIndex - VISIBLE_ROWS + 1;

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setFont(u8g2_font_6x10_tf);

  for (uint8_t row = 0; row < VISIBLE_ROWS; ++row) {
    uint8_t songIndex = firstVisibleSongIndex + row;
    if (songIndex >= mp3.getTrackCount()) break;

    uint8_t y = row * ROW_HEIGHT;
    u8g2.setDrawColor(1);
    if (songIndex == screenState.selectedSongIndex) {
      u8g2.drawBox(0, y, 128, ROW_HEIGHT);
      u8g2.setDrawColor(0);
    }

    // Dot marks the current alarm song
    if (songIndex == deviceState.alarmSongIndex) u8g2.drawFilledEllipse(5, y + 5, 2, 2);
    u8g2.drawStr(12, y + 9, mp3.getSongName(songIndex));
  }

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void OledScreen::drawAlarmSetterScreen(const DeviceState& deviceState, const ScreenState& screenState){
  uint8_t displayHour = screenState.alarmHour % 12 == 0 ? 12 : screenState.alarmHour % 12;
  char hourText[4];
  char minuteText[4];
  snprintf(hourText, sizeof(hourText), "%02u", displayHour);
  snprintf(minuteText, sizeof(minuteText), "%02u", screenState.alarmMinute);

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(10, 17, "Set Alarm");

  u8g2.setFont(u8g2_font_timR24_tr);
  uint8_t hourWidth = u8g2.getStrWidth(hourText);
  uint8_t minuteWidth = u8g2.getStrWidth(minuteText);
  uint8_t minuteX = 10 + hourWidth + u8g2.getStrWidth(":");
  u8g2.drawStr(10, 50, hourText);
  u8g2.drawStr(10 + hourWidth, 50, ":");
  u8g2.drawStr(minuteX, 50, minuteText);

  // Underline the part the knob is changing
  if (screenState.isEditingAlarmMinute) u8g2.drawBox(minuteX, 54, minuteWidth, 2);
  else u8g2.drawBox(10, 54, hourWidth, 2);

  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(minuteX + minuteWidth + 4, 50, screenState.alarmHour < 12 ? "AM" : "PM");

  u8g2.sendBuffer();
}

void OledScreen::drawBrightnessSetterScreen(const DeviceState& deviceState, const ScreenState& screenState){
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(10, 17, "Set LED Brightness");
  char brightnessText[5]; // "100%" + terminator
  snprintf(brightnessText, sizeof(brightnessText), "%u%%", screenState.brightnessPercent);
  u8g2.setFont(u8g2_font_timR24_tr);
  u8g2.drawStr(10, 50, brightnessText);
  u8g2.sendBuffer();
}

void OledScreen::drawVolumeSetterScreen(const DeviceState& deviceState){
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(10, 17, "Set Volume");
  char volumeText[5]; // "100%" + terminator
  snprintf(volumeText, sizeof(volumeText), "%u%%", deviceState.volume * 100 / Mp3Player::maxVolume);
  u8g2.setFont(u8g2_font_timR24_tr);
  u8g2.drawStr(10, 50, volumeText);
  u8g2.sendBuffer();
}

void OledScreen::drawInfoScreen(const DeviceState& deviceState){
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(26, 14, "Sunrise Clock");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(9, 31, "SSID: ");
  u8g2.drawStr(41, 31, deviceState.ssid);
  u8g2.drawStr(9, 43, "BLE :");
  u8g2.drawStr(41, 43, deviceState.bleAdvertiserName);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(10, 60, "Credit to Katia and Lachlan");
  u8g2.sendBuffer();
}

void OledScreen::drawAlarmScreen(const DeviceState& deviceState, const ScreenState& screenState){
  char timeText[6] = "--:--";
  char periodText[3] = "";
  if (deviceState.isTimeSynced) {
    strftime(timeText, sizeof(timeText), "%I:%M", &deviceState.currentTime);
    strftime(periodText, sizeof(periodText), "%p", &deviceState.currentTime);
  }

  float sunriseProgress = screenState.isAlarmRinging ? 1.0 : (millis() - screenState.sunriseStartMs) / float(screenState.sunriseDurationMs);
  if (sunriseProgress > 1.0) sunriseProgress = 1.0;
  uint8_t sunY = HORIZON_Y + SUN_RADIUS - sunriseProgress * (SUN_RADIUS * 2 + 2);

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(2, 11, "Good morning");

  u8g2.setFont(u8g2_font_timR18_tr);
  uint8_t timeWidth = u8g2.getStrWidth(timeText);
  u8g2.drawStr(2, 40, timeText);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(4 + timeWidth, 40, periodText);

  // Clip at the horizon, so the sun is hidden until it rises
  u8g2.setClipWindow(0, 0, 128, HORIZON_Y);
  u8g2.drawDisc(SUN_X, sunY, SUN_RADIUS);
  u8g2.setMaxClipWindow();
  u8g2.drawHLine(84, HORIZON_Y, 44);

  if (screenState.isAlarmRinging) {
    uint8_t rayStart = SUN_RADIUS + RAY_GAP;
    uint8_t rayEnd = rayStart + RAY_LENGTH;
    uint8_t diagonalStart = rayStart * 7 / 10; // Roughly r / sqrt(2), so diagonals sit the same distance out
    uint8_t diagonalEnd = rayEnd * 7 / 10;
    u8g2.drawLine(SUN_X, sunY - rayStart, SUN_X, sunY - rayEnd);
    u8g2.drawLine(SUN_X - rayStart, sunY, SUN_X - rayEnd, sunY);
    u8g2.drawLine(SUN_X + rayStart, sunY, SUN_X + rayEnd, sunY);
    u8g2.drawLine(SUN_X - diagonalStart, sunY - diagonalStart, SUN_X - diagonalEnd, sunY - diagonalEnd);
    u8g2.drawLine(SUN_X + diagonalStart, sunY - diagonalStart, SUN_X + diagonalEnd, sunY - diagonalEnd);
    u8g2.drawXBMP(104, 48, 20, 16, image_volume_loud_bits);
  }

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(2, 62, alarmPhrases[screenState.alarmPhraseIndex]);

  u8g2.sendBuffer();
}

void OledScreen::drawOffScreen(){
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}