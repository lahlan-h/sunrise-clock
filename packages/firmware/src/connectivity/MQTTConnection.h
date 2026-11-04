#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <Peripheral.h>

#include "DeviceState.h"

class CommandRouter;
class WiFiConnection;

// - - - - - - - Usage - - - - - - -
// MQTT adapter for device <id>:
//   sunrise-clock/<id>/telemetry  full DeviceState JSON, on connect, every
//                                 2 minutes, and whenever publishState() is called
//   sunrise-clock/<id>/command    command JSON (see CommandRouter.h)
// Reconnects with backoff (1 s up to 60 s) whenever Wi-Fi is up.
// - - - - - - - - - - - - - - - - -
class MQTTConnection : public Peripheral {
  public:
    MQTTConnection(
        WiFiConnection& wifi,
        const DeviceState& state,
        CommandRouter& router,
        const char* host,
        uint16_t port,
        const char* username,
        const char* password,
        const char* deviceId);

    void begin() override;
    void update() override;  // Keeps the connection alive and publishes on the interval

    bool isConnected();
    void publishState();     // Publish telemetry now, e.g. right after a command

  private:
    WiFiConnection& wifi;
    const DeviceState& state;
    CommandRouter& router;
    const char* host;
    uint16_t port;
    const char* username;
    const char* password;
    const char* deviceId;

    WiFiClient networkClient;
    PubSubClient client;
    String telemetryTopic;
    String commandTopic;

    uint32_t lastAttemptMs = 0;
    uint32_t retryDelayMs = 0;
    uint32_t lastPublishMs = 0;

    void connect();
};
