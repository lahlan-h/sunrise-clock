#include "TelemetryPublisher.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "MQTTConnection.h"

TelemetryPublisher::TelemetryPublisher(
    MQTTConnection& mqtt,
    const DeviceState& state,
    const char* deviceId,
    const char* location,
    uint32_t intervalMs)
    : mqtt(mqtt),
      state(state),
      deviceId(deviceId),
      location(location),
      intervalMs(intervalMs == 0 ? DEFAULT_INTERVAL_MS : intervalMs) {}

void TelemetryPublisher::begin() {
  if (deviceId == nullptr || deviceId[0] == '\0') {
    setError("invalid_telemetry_device_id");
    return;
  }

  started = true;
  nextPublishAt = 0;
}

void TelemetryPublisher::update() {
  if (!started || !mqtt.isConnected()) {
    return;
  }

  const uint32_t now = millis();
  if (stateChanged() || static_cast<int32_t>(now - nextPublishAt) >= 0) {
    publishNow();
  }
}

bool TelemetryPublisher::publishNow() {
  if (!started) {
    setError("telemetry_not_started");
    return false;
  }
  if (!mqtt.isConnected()) {
    ++publishFailures;
    setError("mqtt_not_connected");
    return false;
  }
  if (!buildPayload(lastPayloadBuffer, sizeof(lastPayloadBuffer), true)) {
    ++publishFailures;
    setError("telemetry_payload_too_large");
    return false;
  }
  if (!mqtt.publishTelemetry(lastPayloadBuffer)) {
    ++publishFailures;
    setError("telemetry_publish_failed");
    return false;
  }

  buildPayload(lastSignature, sizeof(lastSignature), false);
  hasPublished = true;
  nextPublishAt = millis() + intervalMs;
  errorMessage[0] = '\0';
  return true;
}

uint32_t TelemetryPublisher::publishFailureCount() const {
  return publishFailures;
}

const char* TelemetryPublisher::lastPayload() const {
  return lastPayloadBuffer;
}

const char* TelemetryPublisher::lastError() const {
  return errorMessage;
}

bool TelemetryPublisher::buildPayload(
    char* output, size_t outputSize, bool includeUptime) const {
  if (output == nullptr || outputSize == 0) {
    return false;
  }

  char temperature[16];
  char humidity[16];
  if (state.temperatureValid && std::isfinite(state.temperatureC)) {
    snprintf(temperature, sizeof(temperature), "%.2f", state.temperatureC);
  } else {
    strcpy(temperature, "null");
  }
  if (state.humidityValid && std::isfinite(state.humidityPercent)) {
    snprintf(humidity, sizeof(humidity), "%.2f", state.humidityPercent);
  } else {
    strcpy(humidity, "null");
  }

  const int written = snprintf(
      output,
      outputSize,
      "{\"protocol_version\":%u,\"device_id\":\"%s\",\"location\":\"%s\","
      "\"temperature_c\":%s,\"temperature_valid\":%s,"
      "\"humidity_percent\":%s,\"humidity_valid\":%s,"
      "\"ambient_light_percent\":%d,"
      "\"motion\":%s,\"button_pressed\":%s,"
      "\"rotary_position\":%ld,\"alarm_enabled\":%s,\"alarm_hour\":%u,"
      "\"alarm_minute\":%u,\"light_on\":%s,\"light_brightness_percent\":%u,"
      "\"light_mode\":\"%s\",\"sound_on\":%s,\"screen\":\"%s\","
      "\"wifi_connected\":%s,\"wifi_rssi\":%d,"
      "\"ble_connected\":%s,\"time_synced\":%s,\"uptime_ms\":%lu}",
      MQTTConnection::PROTOCOL_VERSION,
      deviceId,
      location == nullptr ? "" : location,
      temperature,
      state.temperatureValid ? "true" : "false",
      humidity,
      state.humidityValid ? "true" : "false",
      state.ambientLightPercent,
      state.motionDetected ? "true" : "false",
      state.buttonPressed ? "true" : "false",
      static_cast<long>(state.rotaryPosition),
      state.isAlarmSet ? "true" : "false",
      state.alarmHour,
      state.alarmMinute,
      state.lightOn ? "true" : "false",
      state.lightBrightnessPercent,
      state.lightMode == nullptr ? "UNKNOWN" : state.lightMode,
      state.isSoundOn ? "true" : "false",
      screenName(state.screen),
      state.isWifiConnected ? "true" : "false",
      state.wifiRssi,
      state.isBluetoothConnected ? "true" : "false",
      state.isTimeSynced ? "true" : "false",
      includeUptime ? millis() : 0UL);

  if (written < 0 || static_cast<size_t>(written) >= outputSize) {
    output[0] = '\0';
    return false;
  }
  return true;
}

bool TelemetryPublisher::stateChanged() {
  char signature[MAX_PAYLOAD_SIZE];
  if (!buildPayload(signature, sizeof(signature), false)) {
    return false;
  }
  return !hasPublished || strcmp(signature, lastSignature) != 0;
}

const char* TelemetryPublisher::screenName(Screen screen) {
  switch (screen) {
    case Screen::Home: return "HOME";
    case Screen::Main_Menu: return "MAIN_MENU";
    case Screen::Alarm_Setter: return "ALARM_SETTER";
    case Screen::Song_Picker: return "SONG_PICKER";
    case Screen::Settings: return "SETTINGS";
    case Screen::Off: return "OFF";
  }
  return "UNKNOWN";
}

void TelemetryPublisher::setError(const char* message) {
  if (message == nullptr) {
    errorMessage[0] = '\0';
    return;
  }
  strncpy(errorMessage, message, sizeof(errorMessage) - 1);
  errorMessage[sizeof(errorMessage) - 1] = '\0';
}
