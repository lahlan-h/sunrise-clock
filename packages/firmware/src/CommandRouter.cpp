#include "CommandRouter.h"

#include <ArduinoJson.h>
#include <cstring>

#include "light/LightArray.h"

constexpr uint8_t CommandRouter::PROTOCOL_VERSION;
constexpr size_t CommandRouter::MAX_COMMAND_LENGTH;

namespace {
  const UBaseType_t QUEUE_LENGTH = 4;
  const LightMode LIGHT_MODES[] = {LightMode::Off, LightMode::Steady, LightMode::Sunrise};

  bool parseLightMode(const char* name, LightMode& mode) {
    for (LightMode candidate : LIGHT_MODES) {
      if (strcmp(name, lightModeName(candidate)) == 0) {
        mode = candidate;
        return true;
      }
    }
    return false;
  }
}

CommandRouter::CommandRouter(DeviceState& state, LightArray& lights)
: state(state), lights(lights) {}

void CommandRouter::begin() {
  if (queue == nullptr) queue = xQueueCreate(QUEUE_LENGTH, sizeof(Command));
}

CommandResult CommandRouter::submit(const char* json, size_t length) {
  CommandResult result;
  Command command;
  result.error = parse(json, length, command, result);
  if (result.error != nullptr) return result;

  if (queue == nullptr) {
    result.error = "not_ready";
  } else if (xQueueSend(queue, &command, 0) != pdTRUE) {
    result.error = "queue_full";
  }
  return result;
}

bool CommandRouter::update() {
  if (queue == nullptr) return false;

  bool applied = false;
  Command command;
  while (xQueueReceive(queue, &command, 0) == pdTRUE) {
    apply(command);
    applied = true;
  }
  return applied;
}

const char* CommandRouter::parse(const char* json, size_t length, Command& command, CommandResult& result) {
  if (length > MAX_COMMAND_LENGTH) return "payload_too_large";

  JsonDocument doc;
  if (deserializeJson(doc, json, length)) return "invalid_json";

  // Copy the id first so the client can match up rejections too
  const char* commandId = doc["command_id"] | "";
  if (strlen(commandId) < sizeof(result.commandId)) {
    strlcpy(result.commandId, commandId, sizeof(result.commandId));
  }

  int protocolVersion = doc["protocol_version"] | 0;
  if (protocolVersion != PROTOCOL_VERSION) return "unsupported_protocol";
  if (result.commandId[0] == '\0') return "invalid_command_id";

  const char* type = doc["type"] | "";

  if (strcmp(type, "set_alarm") == 0) {
    int hour = doc["hour"] | -1;
    int minute = doc["minute"] | -1;
    if (!doc["enabled"].is<bool>() || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
      return "invalid_alarm";
    }
    command.type = CommandType::SetAlarm;
    command.alarmEnabled = doc["enabled"].as<bool>();
    command.alarmHour = static_cast<uint8_t>(hour);
    command.alarmMinute = static_cast<uint8_t>(minute);
    return nullptr;
  }

  if (strcmp(type, "set_light") == 0) {
    int brightness = doc["brightness_percent"] | -1;
    if (brightness < 0 || brightness > 100 || !parseLightMode(doc["mode"] | "", command.lightMode)) {
      return "invalid_light";
    }
    command.type = CommandType::SetLight;
    command.brightnessPercent = static_cast<uint8_t>(brightness);
    return nullptr;
  }

  return "unsupported_command";
}

void CommandRouter::apply(const Command& command) {
  switch (command.type) {
    case CommandType::SetAlarm:
      state.isAlarmSet = command.alarmEnabled;
      state.alarmHour = command.alarmHour;
      state.alarmMinute = command.alarmMinute;
      break;
    case CommandType::SetLight:
      lights.applyControl(command.brightnessPercent, command.lightMode);
      break;
  }
}
