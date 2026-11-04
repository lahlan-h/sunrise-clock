#pragma once

#include <Arduino.h>

#include "DeviceState.h"

class MQTTConnection;

class TelemetryPublisher {
  public:
    static constexpr uint32_t DEFAULT_INTERVAL_MS = 120000;
    static constexpr size_t MAX_PAYLOAD_SIZE = 768;

    TelemetryPublisher(
        MQTTConnection& mqtt,
        const DeviceState& state,
        const char* deviceId,
        const char* location = nullptr,
        uint32_t intervalMs = DEFAULT_INTERVAL_MS);

    void begin();
    void update();
    bool publishNow();

    uint32_t publishFailureCount() const;
    const char* lastPayload() const;
    const char* lastError() const;

  private:
    MQTTConnection& mqtt;
    const DeviceState& state;
    const char* deviceId;
    const char* location;
    uint32_t intervalMs;
    uint32_t nextPublishAt = 0;
    uint32_t publishFailures = 0;
    bool started = false;
    bool hasPublished = false;
    char lastPayloadBuffer[MAX_PAYLOAD_SIZE] = {};
    char lastSignature[MAX_PAYLOAD_SIZE] = {};
    char errorMessage[48] = {};

    bool buildPayload(char* output, size_t outputSize, bool includeUptime) const;
    bool stateChanged();
    static const char* screenName(Screen screen);
    void setError(const char* message);
};
