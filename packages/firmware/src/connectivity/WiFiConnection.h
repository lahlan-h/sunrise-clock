#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

class WiFiConnection{
  public:
    WiFiConnection(const char* ssid, const char* password);

    void begin();

    void connect();
    void disconnect();
    
    const char* getSSID() const;

    bool getTime(tm& currentTime) const;
    bool isConnected() const;

  private:
    const char* ssid;
    const char* password;
};