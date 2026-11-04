#include "WiFiConnection.h"

// Sydney, with daylight saving
static const char* const TIME_ZONE = "AEST-10AEDT,M10.1.0,M4.1.0/3";
static const char* const NTP_SERVER = "pool.ntp.org";

static const uint32_t RECONNECT_INTERVAL_MS = 30000;

WiFiConnection::WiFiConnection(const char* ssid, const char* password)
: ssid(ssid), password(password) {}

void WiFiConnection::begin(){
  WiFi.mode(WIFI_STA);
  connect();
  configTzTime(TIME_ZONE, NTP_SERVER); // Configure timezone ESP retrieves
}

// The ESP32 core reconnects after most drops by itself, but gives up on a
// few (e.g. AUTH_FAIL after a router reboot). This catches those.
void WiFiConnection::update(){
  uint32_t now = millis();
  if (!shouldBeConnected || WiFi.isConnected()) {
    lastRetryMs = now; // Restart the countdown while all is well
    return;
  }
  if (now - lastRetryMs < RECONNECT_INTERVAL_MS) return;

  lastRetryMs = now;
  WiFi.reconnect();
}

void WiFiConnection::connect(){
  shouldBeConnected = true;
  WiFi.begin(ssid, password);
}

void WiFiConnection::disconnect(){
  shouldBeConnected = false;
  WiFi.disconnect();
}

int8_t WiFiConnection::getRSSI() const {
  return WiFi.RSSI();
}

const char* WiFiConnection::getSSID() const {
  return ssid;
}

// Non-blocking: getLocalTime() would delay(10) on every call until NTP syncs
bool WiFiConnection::getTime(tm& currentTime) const{
  time_t now = time(nullptr);
  localtime_r(&now, &currentTime);
  return currentTime.tm_year > (2016 - 1900);
}

bool WiFiConnection::isConnected() const {
  return WiFi.isConnected();
}
