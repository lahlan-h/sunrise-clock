#include "BLEConnection.h"

#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <cmath>

#include "CommandRouter.h"

namespace {
const char* const SERVICE_UUID = "75c3662a-606b-417c-a9a2-3f268ef96579";
const char* const STATE_UUID   = "b707bc8f-92f4-4f46-81c2-a34788fd7382";
const char* const COMMAND_UUID = "fbaee8da-4ab7-4a1b-9264-438885e96408";
const char* const INFO_UUID    = "3f52dc07-56d2-4141-b8fa-f7355972d74e";

const uint32_t STATE_REFRESH_MS = 1000;

// Sensor reading rounded to 1 dp, or null when invalid
void setReading(JsonDocument& doc, const char* key, bool valid, float value) {
  if (valid) doc[key] = std::round(value * 10.0) / 10.0;
  else doc[key] = nullptr;
}
}  // namespace

class BLEConnection::ServerCallbacks : public BLEServerCallbacks {
  public:
    explicit ServerCallbacks(BLEConnection& connection) : connection(connection) {}

    void onConnect(BLEServer*) override {
      connection.connected = true;
    }

    void onDisconnect(BLEServer* server) override {
      connection.connected = false;
      server->startAdvertising();  // Let the phone reconnect
    }

  private:
    BLEConnection& connection;
};

class BLEConnection::CommandCallbacks : public BLECharacteristicCallbacks {
  public:
    explicit CommandCallbacks(BLEConnection& connection) : connection(connection) {}

    void onWrite(BLECharacteristic* characteristic) override {
      auto value = characteristic->getValue();
      connection.handleCommand(value.c_str(), value.length());
    }

  private:
    BLEConnection& connection;
};

BLEConnection::BLEConnection(const char* deviceName, const DeviceState& state, CommandRouter& router)
: deviceName(deviceName), state(state), router(router) {}

void BLEConnection::begin() {
  if (stateCharacteristic != nullptr) return;

  BLEDevice::init(deviceName);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks(*this));
  BLEService* service = server->createService(SERVICE_UUID);

  stateCharacteristic = service->createCharacteristic(
      STATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  stateCharacteristic->addDescriptor(new BLE2902());

  commandCharacteristic = service->createCharacteristic(
      COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  commandCharacteristic->addDescriptor(new BLE2902());
  commandCharacteristic->setCallbacks(new CommandCallbacks(*this));

  BLECharacteristic* info = service->createCharacteristic(INFO_UUID, BLECharacteristic::PROPERTY_READ);
  JsonDocument doc;
  doc["device_name"] = deviceName;
  doc["firmware_version"] = state.firmwareVersion;
  doc["protocol_version"] = CommandRouter::PROTOCOL_VERSION;
  char payload[128];
  serializeJson(doc, payload, sizeof(payload));
  info->setValue(payload);

  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  notifyState();  // Gives the state characteristic its first value
}

void BLEConnection::update() {
  if (millis() - lastRefreshMs < STATE_REFRESH_MS) return;
  notifyState();
}

bool BLEConnection::isConnected() const {
  return connected;
}

void BLEConnection::notifyState() {
  if (stateCharacteristic == nullptr) return;
  lastRefreshMs = millis();

  JsonDocument doc;
  doc["alarm_enabled"] = state.isAlarmSet;
  doc["alarm_hour"] = state.alarmHour;
  doc["alarm_minute"] = state.alarmMinute;
  doc["light_mode"] = lightModeName(state.lightMode);
  doc["light_brightness_percent"] = state.lightBrightnessPercent;
  doc["volume"] = state.volume;
  setReading(doc, "temperature_c", state.temperatureValid, state.temperature);
  setReading(doc, "humidity_percent", state.humidityValid, state.humidity);

  char payload[sizeof(lastState)];
  serializeJson(doc, payload, sizeof(payload));
  if (strcmp(payload, lastState) == 0) return;  // Nothing new to tell the phone

  strlcpy(lastState, payload, sizeof(lastState));
  stateCharacteristic->setValue(payload);
  if (connected) stateCharacteristic->notify();
}

void BLEConnection::handleCommand(const char* json, size_t length) {
  CommandResult result = router.submit(json, length);

  JsonDocument ack;
  ack["command_id"] = result.commandId;
  ack["accepted"] = result.error == nullptr;
  if (result.error != nullptr) ack["error"] = result.error;

  char payload[128];
  serializeJson(ack, payload, sizeof(payload));
  commandCharacteristic->setValue(payload);
  commandCharacteristic->notify();
}
