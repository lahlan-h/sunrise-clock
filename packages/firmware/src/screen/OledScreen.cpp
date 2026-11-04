#include "OledScreen.h"
#include "icons.h"

static const uint8_t ROW_HEIGHT = 11;
static const uint8_t VISIBLE_ROWS = 6;

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

void OledScreen::drawSongPickerScreen(const char *const songNames[], uint8_t songCount, uint8_t selectedSong, uint8_t firstVisibleSong) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  for (uint8_t row = 0; row < VISIBLE_ROWS; ++row) {
    const uint8_t songIndex = firstVisibleSong + row;
    if (songIndex >= songCount) {
      break;
    }

    const uint8_t y = row * ROW_HEIGHT;
    if (songIndex == selectedSong) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, y, 128, ROW_HEIGHT);
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }

    u8g2.drawStr(2, y + 9, songNames[songIndex]);
  }

  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void OledScreen::drawBrightnessSetterScreen(const DeviceState& deviceState){
   
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

void OledScreen::drawOffScreen(){
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}