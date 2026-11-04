#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include <Connections.h>

class WiFiConnection : public Connections {
  public:
    WiFiConnection(const char* ssid, const char* password);

    void begin() override;

    void connect();
    void disconnect();
    
    const char* getSSID() const;

    bool getTime(tm& currentTime) const;
    bool isConnected() const;

  private:
    const char* ssid;
    const char* password;
};