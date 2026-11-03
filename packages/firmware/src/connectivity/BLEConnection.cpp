#include "BLEConnection.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include "DeviceState.h"

constexpr char BLEConnection::SERVICE_UUID[];
constexpr char BLEConnection::STATUS_UUID[];
constexpr char BLEConnection::ALARM_CONFIG_UUID[];
constexpr char BLEConnection::LIGHT_CONTROL_UUID[];
constexpr char BLEConnection::DEVICE_INFO_UUID[];
constexpr char BLEConnection::ACK_UUID[];

namespace {
const size_t MAX_COMMAND_LENGTH = 192;
const uint8_t BLUETOOTH_CONNECTED_EFFECT = 6;

bool readJsonString(const String& payload, const char* key, char* output, size_t outputSize) {
  String marker = String("\"") + key + "\":\"";
  int start = payload.indexOf(marker);
  if (start < 0) return false;
  start += marker.length();
  int end = payload.indexOf('"', start);
  if (end < 0 || static_cast<size_t>(end - start) >= outputSize) return false;
  payload.substring(start, end).toCharArray(output, outputSize);
  return true;
}

bool readJsonInteger(const String& payload, const char* key, int& value) {
  String marker = String("\"") + key + "\":";
  int start = payload.indexOf(marker);
  if (start < 0) return false;
  start += marker.length();
  while (start < payload.length() && payload[start] == ' ') ++start;
  int end = start;
  if (end < payload.length() && payload[end] == '-') ++end;
  int digits = end;
  while (end < payload.length() && isdigit(payload[end])) ++end;
  if (end == digits) return false;
  value = payload.substring(start, end).toInt();
  return true;
}

bool readJsonBoolean(const String& payload, const char* key, bool& value) {
  String marker = String("\"") + key + "\":";
  int start = payload.indexOf(marker);
  if (start < 0) return false;
  start += marker.length();
  while (start < payload.length() && payload[start] == ' ') ++start;
  if (payload.startsWith("true", start)) {
    value = true;
    return true;
  }
  if (payload.startsWith("false", start)) {
    value = false;
    return true;
  }
  return false;
}

bool readProtocolVersion(const String& payload) {
  int protocolVersion = 0;
  return readJsonInteger(payload, "protocol_version", protocolVersion) &&
         protocolVersion == BLEConnection::PROTOCOL_VERSION;
}
}

class BLEConnection::ServerCallbacks : public BLEServerCallbacks {
  public:
    explicit ServerCallbacks(BLEConnection& connection) : connection(connection) {}

    void onConnect(BLEServer*) override {
      connection.clientConnected = true;
      connection.state.isBluetoothConnected = true;
      connection.mp3.playEffect(BLUETOOTH_CONNECTED_EFFECT);
      connection.notifyState();
    }

    void onDisconnect(BLEServer* server) override {
      connection.clientConnected = false;
      connection.state.isBluetoothConnected = false;
      server->startAdvertising();
    }

  private:
    BLEConnection& connection;
};

class BLEConnection::CharacteristicCallbacks : public BLECharacteristicCallbacks {
  public:
    explicit CharacteristicCallbacks(BLEConnection& connection) : connection(connection) {}

    void onRead(BLECharacteristic*) override {
      connection.refreshReadCharacteristics();
    }

    void onWrite(BLECharacteristic* characteristic) override {
      String payload = characteristic->getValue().c_str();
      if (payload.length() > MAX_COMMAND_LENGTH) {
        connection.sendAcknowledgement("", false, "payload_too_large");
        return;
      }

      if (characteristic == static_cast<BLECharacteristic*>(
                                connection.alarmConfigCharacteristic)) {
        connection.handleAlarmWrite(payload);
      } else if (characteristic == static_cast<BLECharacteristic*>(
                                connection.lightControlCharacteristic)) {
        connection.handleLightWrite(payload);
      }
    }

  private:
    BLEConnection& connection;
};

BLEConnection::BLEConnection(const char* deviceName, DeviceState& state, Mp3Player& mp3)
    : deviceName(deviceName),
      state(state),
      mp3(mp3),
      server(nullptr),
      statusCharacteristic(nullptr),
      alarmConfigCharacteristic(nullptr),
      lightControlCharacteristic(nullptr),
      deviceInfoCharacteristic(nullptr),
      acknowledgementCharacteristic(nullptr),
      started(false),
      clientConnected(false),
      queueHead(0),
      queueTail(0),
      queueSize(0) {}

void BLEConnection::begin() {
  if (started) return;

  BLEDevice::init(deviceName);
  BLEServer* bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks(*this));
  BLEService* service = bleServer->createService(SERVICE_UUID);

  BLECharacteristic* status = service->createCharacteristic(
      STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  status->addDescriptor(new BLE2902());
  BLECharacteristic* alarm = service->createCharacteristic(
      ALARM_CONFIG_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);
  alarm->addDescriptor(new BLE2902());
  BLECharacteristic* light = service->createCharacteristic(
      LIGHT_CONTROL_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  light->addDescriptor(new BLE2902());
  BLECharacteristic* info = service->createCharacteristic(
      DEVICE_INFO_UUID, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic* ack = service->createCharacteristic(
      ACK_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  ack->addDescriptor(new BLE2902());

  CharacteristicCallbacks* callbacks = new CharacteristicCallbacks(*this);
  status->setCallbacks(callbacks);
  alarm->setCallbacks(callbacks);
  light->setCallbacks(callbacks);
  info->setCallbacks(callbacks);

  server = bleServer;
  statusCharacteristic = status;
  alarmConfigCharacteristic = alarm;
  lightControlCharacteristic = light;
  deviceInfoCharacteristic = info;
  acknowledgementCharacteristic = ack;

  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();
  started = true;
  refreshReadCharacteristics();
}

void BLEConnection::update() {
  if (!started) return;
  refreshReadCharacteristics();
}

bool BLEConnection::isConnected() const {
  return clientConnected;
}

bool BLEConnection::hasPendingCommand() const {
  return queueSize != 0;
}

bool BLEConnection::popCommand(BLECommand& command) {
  if (queueSize == 0) return false;
  command = commandQueue[queueHead];
  queueHead = (queueHead + 1) % 4;
  --queueSize;
  return true;
}

bool BLEConnection::enqueue(const BLECommand& command) {
  if (queueSize == 4) return false;
  commandQueue[queueTail] = command;
  queueTail = (queueTail + 1) % 4;
  ++queueSize;
  return true;
}

bool BLEConnection::parseAlarmCommand(
    const String& payload, BLECommand& command, String& error) const {
  if (!readProtocolVersion(payload)) {
    error = "unsupported_protocol";
    return false;
  }
  if (!readJsonString(payload, "command_id", command.commandId, sizeof(command.commandId))) {
    error = "missing_command_id";
    return false;
  }
  if (!readJsonBoolean(payload, "enabled", command.alarmEnabled)) {
    error = "invalid_enabled";
    return false;
  }
  int hour = 0;
  int minute = 0;
  if (!readJsonInteger(payload, "hour", hour) || hour < 0 || hour > 23) {
    error = "invalid_hour";
    return false;
  }
  if (!readJsonInteger(payload, "minute", minute) || minute < 0 || minute > 59) {
    error = "invalid_minute";
    return false;
  }
  command.type = BLECommandType::SetAlarm;
  command.alarmHour = static_cast<uint8_t>(hour);
  command.alarmMinute = static_cast<uint8_t>(minute);
  return true;
}

bool BLEConnection::parseLightCommand(
    const String& payload, BLECommand& command, String& error) const {
  if (!readProtocolVersion(payload)) {
    error = "unsupported_protocol";
    return false;
  }
  if (!readJsonString(payload, "command_id", command.commandId, sizeof(command.commandId))) {
    error = "missing_command_id";
    return false;
  }
  int brightness = 0;
  if (!readJsonInteger(payload, "brightness_percent", brightness) ||
      brightness < 0 || brightness > 100) {
    error = "invalid_brightness";
    return false;
  }
  if (!readJsonString(payload, "mode", command.lightMode, sizeof(command.lightMode))) {
    error = "missing_mode";
    return false;
  }
  command.type = BLECommandType::SetLight;
  command.brightnessPercent = static_cast<uint8_t>(brightness);
  return true;
}

void BLEConnection::handleAlarmWrite(const String& payload) {
  BLECommand command;
  String error;
  if (!parseAlarmCommand(payload, command, error)) {
    sendAcknowledgement("", false, error.c_str());
    return;
  }
  if (!enqueue(command)) {
    sendAcknowledgement(command.commandId, false, "command_queue_full");
    return;
  }
  sendAcknowledgement(command.commandId, true, "queued");
}

void BLEConnection::handleLightWrite(const String& payload) {
  BLECommand command;
  String error;
  if (!parseLightCommand(payload, command, error)) {
    sendAcknowledgement("", false, error.c_str());
    return;
  }
  if (!enqueue(command)) {
    sendAcknowledgement(command.commandId, false, "command_queue_full");
    return;
  }
  sendAcknowledgement(command.commandId, true, "queued");
}

void BLEConnection::sendAcknowledgement(
    const char* commandId, bool accepted, const char* message) {
  if (!acknowledgementCharacteristic) return;
  String payload = String("{\"protocol_version\":1,\"command_id\":\"") +
                   commandId + "\",\"accepted\":" + (accepted ? "true" : "false") +
                   ",\"message\":\"" + message + "\"}";
  BLECharacteristic* characteristic =
      static_cast<BLECharacteristic*>(acknowledgementCharacteristic);
  characteristic->setValue(payload.c_str());
  characteristic->notify();
}

void BLEConnection::refreshReadCharacteristics() {
  if (!started) return;
  String status = String("{\"protocol_version\":1,\"ble_connected\":") +
                  (clientConnected ? "true" : "false") +
                  ",\"wifi_connected\":" + (state.isWifiConnected ? "true" : "false") +
                  ",\"alarm_enabled\":" + (state.isAlarmSet ? "true" : "false") +
                  ",\"light_on\":" + (state.lightOn ? "true" : "false") +
                  ",\"light_brightness_percent\":" + state.lightBrightnessPercent + "}";
  static_cast<BLECharacteristic*>(statusCharacteristic)->setValue(status.c_str());

  String alarm = String("{\"protocol_version\":1,\"enabled\":") +
                 (state.isAlarmSet ? "true" : "false") +
                 ",\"hour\":" + state.alarmHour +
                 ",\"minute\":" + state.alarmMinute + "}";
  static_cast<BLECharacteristic*>(alarmConfigCharacteristic)->setValue(alarm.c_str());

  String info = String("{\"protocol_version\":1,\"device_name\":\"") +
                deviceName + "\",\"firmware_version\":\"1.0.0\"}";
  static_cast<BLECharacteristic*>(deviceInfoCharacteristic)->setValue(info.c_str());
}

void BLEConnection::notifyState() {
  refreshReadCharacteristics();
  if (statusCharacteristic) {
    static_cast<BLECharacteristic*>(statusCharacteristic)->notify();
  }
}

void BLEConnection::notifyAlarmConfig() {
  refreshReadCharacteristics();
  if (alarmConfigCharacteristic) {
    static_cast<BLECharacteristic*>(alarmConfigCharacteristic)->notify();
  }
}

void BLEConnection::notifyLightControl() {
  refreshReadCharacteristics();
  if (lightControlCharacteristic) {
    static_cast<BLECharacteristic*>(lightControlCharacteristic)->notify();
  }
}
