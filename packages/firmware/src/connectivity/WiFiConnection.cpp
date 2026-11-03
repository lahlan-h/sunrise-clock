#include "WiFiConnection.h"

// Sydney, with daylight saving
static const char* const TIME_ZONE = "AEST-10AEDT,M10.1.0,M4.1.0/3";
static const char* const NTP_SERVER = "pool.ntp.org";

WiFiConnection::WiFiConnection(const char* ssid, const char* password)
: ssid(ssid), password(password) {}

void WiFiConnection::begin(){  
  WiFi.mode(WIFI_STA); 
  connect();
  configTzTime(TIME_ZONE, NTP_SERVER); // Configure timezone ESP retrieves
}

void WiFiConnection::connect(){
  WiFi.begin(ssid, password);
}

void WiFiConnection::disconnect(){
  WiFi.disconnect();
}

const char* WiFiConnection::getSSID() const {
  return ssid;
}

bool WiFiConnection::getTime(tm& currentTime) const{
  return getLocalTime(&currentTime, 0);
}

bool WiFiConnection::isConnected() const {
  return WiFi.isConnected();
}