#include "MQTTConnection.h"

#include <ArduinoJson.h>
#include <cmath>

#include "CommandRouter.h"
#include "WiFiConnection.h"

namespace {
const uint32_t TELEMETRY_INTERVAL_MS = 30000;
const uint32_t MIN_RETRY_MS = 1000;
const uint32_t MAX_RETRY_MS = 60000;
const uint16_t BUFFER_SIZE = 768;  // Telemetry JSON + topic + MQTT header

// Sensor reading rounded to 1 dp, or null when invalid
void setReading(JsonDocument& doc, const char* key, bool valid, float value) {
  if (valid) doc[key] = std::round(value * 10.0) / 10.0;
  else doc[key] = nullptr;
}
}  // namespace

MQTTConnection::MQTTConnection(
    WiFiConnection& wifi,
    const DeviceState& state,
    CommandRouter& router,
    const char* host,
    uint16_t port,
    const char* username,
    const char* password,
    const char* deviceId)
: wifi(wifi),
  state(state),
  router(router),
  host(host),
  port(port),
  username(username),
  password(password),
  deviceId(deviceId),
  client(networkClient),
  telemetryTopic(String("sunrise-clock/") + deviceId + "/telemetry"),
  commandTopic(String("sunrise-clock/") + deviceId + "/command") {}

void MQTTConnection::begin() {
  client.setServer(host, port);
  client.setBufferSize(BUFFER_SIZE);
  client.setSocketTimeout(2); // PubSubClient's 15 s default freezes loop() whenever a broker drops the connection
  client.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
    if (commandTopic != topic) return;
    CommandResult result = router.submit(reinterpret_cast<const char*>(payload), length);
    if (result.error != nullptr) {
      Serial.printf("[mqtt] command '%s' rejected: %s\n", result.commandId, result.error);
    }
  });
}

void MQTTConnection::update() {
  if (client.connected()) {
    client.loop();
    if (millis() - lastPublishMs >= TELEMETRY_INTERVAL_MS) publishState();
    return;
  }

  if (!wifi.isConnected() || millis() - lastAttemptMs < retryDelayMs) return;
  connect();
}

bool MQTTConnection::isConnected() {
  return client.connected();
}

void MQTTConnection::connect() {
  lastAttemptMs = millis();

  bool connected = (username != nullptr && username[0] != '\0')
      ? client.connect(deviceId, username, password)
      : client.connect(deviceId);

  if (!connected) {
    retryDelayMs = retryDelayMs == 0 ? MIN_RETRY_MS : retryDelayMs * 2;
    if (retryDelayMs > MAX_RETRY_MS) retryDelayMs = MAX_RETRY_MS;
    return;
  }

  retryDelayMs = 0;
  client.subscribe(commandTopic.c_str());
  publishState();  // Announce the current state on every (re)connect
}

void MQTTConnection::publishState() {
  if (!client.connected()) return;
  lastPublishMs = millis();

  JsonDocument doc;
  doc["protocolVersion"] = CommandRouter::PROTOCOL_VERSION;
  doc["deviceId"] = deviceId;
  doc["firmwareVersion"] = state.firmwareVersion;
  setReading(doc, "temperature", state.temperatureValid, state.temperature);
  doc["temperatureValid"] = state.temperatureValid;
  setReading(doc, "humidity", state.humidityValid, state.humidity);
  doc["humidityValid"] = state.humidityValid;
  doc["ambientLightPercent"] = state.ambientLightPercent;
  doc["motionDetected"] = state.motionDetected;
  doc["rotaryButtonPressed"] = state.rotaryButtonPressed;
  doc["backButtonPressed"] = state.backButtonPressed;
  doc["alarmButtonPressed"] = state.alarmButtonPressed;
  doc["rotaryPositionSinceLastReset"] = state.rotaryPositionSinceLastReset;
  doc["isAlarmSet"] = state.isAlarmSet;
  doc["alarmHour"] = state.alarmHour;
  doc["alarmMinute"] = state.alarmMinute;
  doc["isLightOn"] = state.isLightOn;
  doc["lightBrightnessPercent"] = state.lightBrightnessPercent;
  doc["lightMode"] = lightModeName(state.lightMode);
  doc["isSoundOn"] = state.isSoundOn;
  doc["volume"] = state.volume;
  doc["screen"] = screenName(state.screen);
  doc["isWifiConnected"] = state.isWifiConnected;
  doc["wifiRssi"] = state.wifiRssi;
  doc["isBluetoothConnected"] = state.isBluetoothConnected;
  doc["isTimeSynced"] = state.isTimeSynced;
  doc["upTimeMs"] = state.upTimeMs;

  char payload[BUFFER_SIZE];
  serializeJson(doc, payload, sizeof(payload));
  if (!client.publish(telemetryTopic.c_str(), payload)) {
    Serial.println("[mqtt] telemetry publish failed");
  }
}
