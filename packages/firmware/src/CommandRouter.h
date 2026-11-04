#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "DeviceState.h"

class LightArray;

// Outcome of CommandRouter::submit(). error == nullptr means the command was queued.
struct CommandResult {
  char commandId[33] = {};
  const char* error = nullptr;
};

// - - - - - - - Usage - - - - - - -
// One entry point for commands from every transport (BLE, MQTT, ...).
// Transports hand over the raw JSON; the router validates and queues it,
// then applies it from loop(). Every transport uses the same JSON:
//
//   {"protocol_version":1,"command_id":"a-1","type":"set_alarm","enabled":true,"hour":6,"minute":30}
//   {"protocol_version":1,"command_id":"l-1","type":"set_light","brightness_percent":50,"mode":"STEADY"}
//
// mode is OFF, STEADY or SUNRISE.
// - - - - - - - - - - - - - - - - -
class CommandRouter {
  public:
    static constexpr uint8_t PROTOCOL_VERSION = 1;
    static constexpr size_t MAX_COMMAND_LENGTH = 256;

    CommandRouter(DeviceState& state, LightArray& lights);

    void begin();  // Call before any transport can deliver commands

    // Validate and queue a command. Safe to call from any task (BLE callbacks run on core 0)
    CommandResult submit(const char* json, size_t length);

    // Apply queued commands. Call from loop(); returns true if any were applied
    bool update();

  private:
    enum class CommandType : uint8_t { SetAlarm, SetLight };

    struct Command {
      CommandType type = CommandType::SetAlarm;
      bool alarmEnabled = false;
      uint8_t alarmHour = 0;
      uint8_t alarmMinute = 0;
      uint8_t brightnessPercent = 0;
      LightMode lightMode = LightMode::Off;
    };

    DeviceState& state;
    LightArray& lights;
    QueueHandle_t queue = nullptr;

    static const char* parse(const char* json, size_t length, Command& command, CommandResult& result);
    void apply(const Command& command);
};
