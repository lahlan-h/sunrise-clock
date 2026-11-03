#include "MQTTConnection.h"

#include <cstdlib>
#include <cstring>

#include "WiFiConnection.h"

MQTTConnection* MQTTConnection::activeConnection = nullptr;

MQTTConnection::MQTTConnection(
    WiFiConnection& wifi,
    const char* host,
    uint16_t port,
    const char* username,
    const char* password,
    const char* deviceId,
    bool useTls,
    const char* rootCertificate)
    : wifi(wifi),
      host(host),
      port(port),
      username(username),
      password(password),
      deviceId(deviceId),
      useTls(useTls),
      rootCertificate(rootCertificate),
      mqttClient(networkClient),
      clientId(String("sunrise-clock-") + (deviceId == nullptr ? "" : deviceId)),
      telemetryTopicName(String("sunrise-clock/") +
                         (deviceId == nullptr ? "" : deviceId) + "/telemetry"),
      commandTopicName(String("sunrise-clock/") +
                       (deviceId == nullptr ? "" : deviceId) + "/command") {}

void MQTTConnection::begin() {
  if (host == nullptr || host[0] == '\0' || deviceId == nullptr ||
      deviceId[0] == '\0') {
    setError("invalid_mqtt_configuration");
    return;
  }

  if (useTls) {
    if (rootCertificate == nullptr || rootCertificate[0] == '\0') {
      setError("missing_mqtt_root_certificate");
      return;
    }
    networkClient.setCACert(rootCertificate);
  }

  mqttClient.setServer(host, port);
  mqttClient.setCallback(MQTTConnection::messageCallback);
  mqttClient.setBufferSize(MAX_PAYLOAD_SIZE);
  activeConnection = this;
  started = true;
  nextReconnectAt = 0;
}

void MQTTConnection::update() {
  if (!started) {
    return;
  }

  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }

  connectIfReady();
}

void MQTTConnection::connectIfReady() {
  if (!wifi.isConnected() || millis() < nextReconnectAt) {
    return;
  }

  bool connected = false;
  if (username != nullptr && username[0] != '\0') {
    connected = mqttClient.connect(clientId.c_str(), username, password);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }

  if (!connected) {
    setError("mqtt_connect_failed");
    nextReconnectAt = millis() + reconnectDelay;
    if (reconnectDelay < 60000) {
      reconnectDelay *= 2;
      if (reconnectDelay > 60000) {
        reconnectDelay = 60000;
      }
    }
    return;
  }

  reconnectDelay = 1000;
  nextReconnectAt = 0;
  if (!mqttClient.subscribe(commandTopicName.c_str())) {
    setError("mqtt_subscribe_failed");
  } else {
    errorMessage[0] = '\0';
  }
}

bool MQTTConnection::isConnected() const {
  return mqttClient.connected();
}

bool MQTTConnection::publishTelemetry(const char* payload, bool retained) {
  if (!mqttClient.connected() || payload == nullptr ||
      strlen(payload) >= MAX_PAYLOAD_SIZE) {
    ++publishFailures;
    setError("telemetry_publish_rejected");
    return false;
  }
  if (!mqttClient.publish(telemetryTopicName.c_str(), payload, retained)) {
    ++publishFailures;
    setError("telemetry_publish_failed");
    return false;
  }
  return true;
}

bool MQTTConnection::popCommand(BLECommand& command) {
  if (queueSize == 0) {
    return false;
  }
  command = commandQueue[(queueHead + COMMAND_QUEUE_CAPACITY - queueSize) %
                         COMMAND_QUEUE_CAPACITY];
  --queueSize;
  return true;
}

const char* MQTTConnection::telemetryTopic() const {
  return telemetryTopicName.c_str();
}

const char* MQTTConnection::commandTopic() const {
  return commandTopicName.c_str();
}

const char* MQTTConnection::lastError() const {
  return errorMessage;
}

uint32_t MQTTConnection::publishFailureCount() const {
  return publishFailures;
}

void MQTTConnection::messageCallback(char* topic, byte* payload,
                                     unsigned int length) {
  if (activeConnection != nullptr) {
    activeConnection->handleMessage(topic, payload, length);
  }
}

void MQTTConnection::handleMessage(const char* topic, const byte* payload,
                                   unsigned int length) {
  if (topic == nullptr || strcmp(topic, commandTopicName.c_str()) != 0) {
    return;
  }

  BLECommand command = {};
  if (parseCommand(reinterpret_cast<const char*>(payload), length, command) &&
      enqueue(command)) {
    return;
  }
}

bool MQTTConnection::enqueue(const BLECommand& command) {
  if (queueSize == COMMAND_QUEUE_CAPACITY) {
    setError("command_queue_full");
    return false;
  }
  commandQueue[queueHead] = command;
  queueHead = (queueHead + 1) % COMMAND_QUEUE_CAPACITY;
  ++queueSize;
  return true;
}

bool MQTTConnection::parseCommand(const char* payload, size_t length,
                                  BLECommand& command) {
  if (payload == nullptr || length == 0 || length >= MAX_PAYLOAD_SIZE) {
    setError("invalid_command_payload");
    return false;
  }

  char data[MAX_PAYLOAD_SIZE];
  memcpy(data, payload, length);
  data[length] = '\0';

  int protocolVersion = 0;
  if (!readInteger(data, length, "protocol_version", protocolVersion) ||
      protocolVersion != PROTOCOL_VERSION ||
      !readString(data, length, "command_id", command.commandId,
                  sizeof(command.commandId))) {
    setError("invalid_command_header");
    return false;
  }

  char type[17] = {};
  if (!readString(data, length, "type", type, sizeof(type))) {
    setError("missing_command_type");
    return false;
  }

  if (strcmp(type, "set_alarm") == 0) {
    int hour = 0;
    int minute = 0;
    if (!readBoolean(data, length, "enabled", command.alarmEnabled) ||
        !readInteger(data, length, "hour", hour) ||
        !readInteger(data, length, "minute", minute) || hour < 0 ||
        hour > 23 || minute < 0 || minute > 59) {
      setError("invalid_alarm_command");
      return false;
    }
    command.type = BLECommandType::SetAlarm;
    command.alarmHour = static_cast<uint8_t>(hour);
    command.alarmMinute = static_cast<uint8_t>(minute);
    return true;
  }

  if (strcmp(type, "set_light") == 0) {
    int brightness = 0;
    if (!readInteger(data, length, "brightness_percent", brightness) ||
        brightness < 0 || brightness > 100 ||
        !readString(data, length, "mode", command.lightMode,
                    sizeof(command.lightMode))) {
      setError("invalid_light_command");
      return false;
    }
    command.type = BLECommandType::SetLight;
    command.brightnessPercent = static_cast<uint8_t>(brightness);
    return true;
  }

  setError("unsupported_command");
  return false;
}

bool MQTTConnection::readString(const char* payload, size_t length,
                                const char* key, char* output,
                                size_t outputSize) const {
  String marker = String("\"") + key + "\":\"";
  const char* start = strstr(payload, marker.c_str());
  if (start == nullptr) {
    return false;
  }
  start += marker.length();
  const char* end = strchr(start, '"');
  if (end == nullptr || static_cast<size_t>(end - payload) >= length ||
      static_cast<size_t>(end - start) >= outputSize) {
    return false;
  }
  memcpy(output, start, end - start);
  output[end - start] = '\0';
  return true;
}

bool MQTTConnection::readInteger(const char* payload, size_t length,
                                 const char* key, int& output) const {
  String marker = String("\"") + key + "\":";
  const char* start = strstr(payload, marker.c_str());
  if (start == nullptr || static_cast<size_t>(start - payload) >= length) {
    return false;
  }
  start += marker.length();
  char* end = nullptr;
  long value = strtol(start, &end, 10);
  if (end == start || value < -2147483647L || value > 2147483647L) {
    return false;
  }
  output = static_cast<int>(value);
  return true;
}

bool MQTTConnection::readBoolean(const char* payload, size_t length,
                                 const char* key, bool& output) const {
  String marker = String("\"") + key + "\":";
  const char* start = strstr(payload, marker.c_str());
  if (start == nullptr || static_cast<size_t>(start - payload) >= length) {
    return false;
  }
  start += marker.length();
  if (strncmp(start, "true", 4) == 0) {
    output = true;
    return true;
  }
  if (strncmp(start, "false", 5) == 0) {
    output = false;
    return true;
  }
  return false;
}

void MQTTConnection::setError(const char* message) {
  strncpy(errorMessage, message, sizeof(errorMessage) - 1);
  errorMessage[sizeof(errorMessage) - 1] = '\0';
}
