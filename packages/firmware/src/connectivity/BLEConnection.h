#pragma once

#include <Arduino.h>

#include <Connections.h>

#include "audio/Mp3Player.h"

struct DeviceState;

enum class BLECommandType : uint8_t {
  SetAlarm,
  SetLight
};

struct BLECommand {
  BLECommandType type = BLECommandType::SetAlarm;
  char commandId[33] = {};
  bool alarmEnabled = false;
  uint8_t alarmHour = 0;
  uint8_t alarmMinute = 0;
  uint8_t brightnessPercent = 0;
  char lightMode[17] = {};
};

class BLEConnection : public Connections {
  public:
    static constexpr uint8_t PROTOCOL_VERSION = 1;
    static constexpr char SERVICE_UUID[] = "75c3662a-606b-417c-a9a2-3f268ef96579";
    static constexpr char STATUS_UUID[] = "b707bc8f-92f4-4f46-81c2-a34788fd7382";
    static constexpr char ALARM_CONFIG_UUID[] = "0a8b765b-73ac-4813-8441-dbe23e992974";
    static constexpr char LIGHT_CONTROL_UUID[] = "7292c4d1-c619-4075-8954-0535d9d1aebf";
    static constexpr char DEVICE_INFO_UUID[] = "3f52dc07-56d2-4141-b8fa-f7355972d74e";
    static constexpr char ACK_UUID[] = "804687da-761a-42ec-b67b-a4c635ef9f85";

    BLEConnection(const char* deviceName, DeviceState& state, Mp3Player& mp3);

    void begin() override;
    void update() override;
    bool isConnected() const;

    bool hasPendingCommand() const;
    bool popCommand(BLECommand& command);

    void notifyState();
    void notifyAlarmConfig();
    void notifyLightControl();
    void acknowledgeCommand(const char* commandId, bool accepted, const char* message);

  private:
    class ServerCallbacks;
    class CharacteristicCallbacks;

    const char* deviceName;
    DeviceState& state;
    Mp3Player& mp3;
    void* server;
    void* statusCharacteristic;
    void* alarmConfigCharacteristic;
    void* lightControlCharacteristic;
    void* deviceInfoCharacteristic;
    void* acknowledgementCharacteristic;
    bool started;
    bool clientConnected;
    BLECommand commandQueue[4];
    uint8_t queueHead;
    uint8_t queueTail;
    uint8_t queueSize;

    bool enqueue(const BLECommand& command);
    bool parseAlarmCommand(const String& payload, BLECommand& command, String& error) const;
    bool parseLightCommand(const String& payload, BLECommand& command, String& error) const;
    void handleAlarmWrite(const String& payload);
    void handleLightWrite(const String& payload);
    void sendAcknowledgement(const char* commandId, bool accepted, const char* message);
    void refreshReadCharacteristics();

    friend class CharacteristicCallbacks;
    friend class ServerCallbacks;
};