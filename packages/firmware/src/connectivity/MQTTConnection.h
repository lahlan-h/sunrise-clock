#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "BLEConnection.h"

class WiFiConnection;

class MQTTConnection {
  public:
    static constexpr uint8_t PROTOCOL_VERSION = 1;
    static constexpr size_t MAX_PAYLOAD_SIZE = 512;
    static constexpr uint8_t COMMAND_QUEUE_CAPACITY = 4;

    MQTTConnection(
        WiFiConnection& wifi,
        const char* host,
        uint16_t port,
        const char* username,
        const char* password,
        const char* deviceId,
        bool useTls = false,
        const char* rootCertificate = nullptr);

    void begin();
    void update();

    bool isConnected() const;
    bool publishTelemetry(const char* payload, bool retained = false);
    bool publishState(const char* payload, bool retained = true);

    bool hasPendingCommand() const;
    bool popCommand(BLECommand& command);

    const char* telemetryTopic() const;
    const char* stateTopic() const;
    const char* commandTopic() const;
    const char* acknowledgementTopic() const;
    const char* lastError() const;
    uint32_t publishFailureCount() const;

  private:
    static MQTTConnection* activeConnection;
    static void messageCallback(char* topic, byte* payload, unsigned int length);

    WiFiConnection& wifi;
    const char* host;
    uint16_t port;
    const char* username;
    const char* password;
    const char* deviceId;
    bool useTls;
    const char* rootCertificate;
    WiFiClientSecure networkClient;
    PubSubClient mqttClient;
    String clientId;
    String telemetryTopicName;
    String stateTopicName;
    String commandTopicName;
    String acknowledgementTopicName;
    BLECommand commandQueue[COMMAND_QUEUE_CAPACITY];
    uint8_t queueHead = 0;
    uint8_t queueTail = 0;
    uint8_t queueSize = 0;
    uint32_t nextReconnectAt = 0;
    uint32_t reconnectDelay = 1000;
    uint32_t publishFailures = 0;
    char errorMessage[48] = {};
    bool started = false;

    void connectIfReady();
    bool enqueue(const BLECommand& command);
    void handleMessage(const char* topic, const byte* payload, unsigned int length);
    bool parseCommand(const char* payload, size_t length, BLECommand& command);
    bool readString(const char* payload, size_t length, const char* key,
                    char* output, size_t outputSize) const;
    bool readInteger(const char* payload, size_t length, const char* key,
                     int& output) const;
    bool readBoolean(const char* payload, size_t length, const char* key,
                     bool& output) const;
    void acknowledge(const char* commandId, bool accepted, const char* message);
    void setError(const char* message);
};
