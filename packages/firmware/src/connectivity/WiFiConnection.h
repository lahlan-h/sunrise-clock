#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include <Peripheral.h>

class WiFiConnection : public Peripheral {
  public:
    WiFiConnection(const char* ssid, const char* password);

    void begin() override;
    void update() override;  // Retries every 30 s if the core's auto-reconnect gives up

    void connect();
    void disconnect();

    int8_t getRSSI() const;

    const char* getSSID() const;

    bool getTime(tm& currentTime) const;
    bool isConnected() const;

  private:
    const char* ssid;
    const char* password;
    bool shouldBeConnected = false;
    uint32_t lastRetryMs = 0;
};
