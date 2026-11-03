#pragma once

#include <Arduino.h>

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

class BLEConnection {
  public:
    static constexpr uint8_t PROTOCOL_VERSION = 1;
    static constexpr char SERVICE_UUID[] = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr char STATUS_UUID[] = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr char ALARM_CONFIG_UUID[] = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr char LIGHT_CONTROL_UUID[] = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr char DEVICE_INFO_UUID[] = "6e400005-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr char ACK_UUID[] = "6e400006-b5a3-f393-e0a9-e50e24dcca9e";

    BLEConnection(const char* deviceName, DeviceState& state);

    void begin();
    void update();
    bool isConnected() const;

    bool hasPendingCommand() const;
    bool popCommand(BLECommand& command);

    void notifyState();
    void notifyAlarmConfig();
    void notifyLightControl();

  private:
    class ServerCallbacks;
    class CharacteristicCallbacks;

    const char* deviceName;
    DeviceState& state;
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