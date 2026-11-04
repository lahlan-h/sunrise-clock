#include "CommandRouter.h"

CommandRouter::CommandRouter(
    DeviceState& state,
    BLEConnection& ble,
    MQTTConnection& mqtt,
    LightArray& lights)
    : state(state), ble(ble), mqtt(mqtt), lights(lights) {}

void CommandRouter::update() {
  BLECommand command;
  while (ble.popCommand(command)) {
    apply(command, CommandOrigin::Ble);
  }
  while (mqtt.popCommand(command)) {
    apply(command, CommandOrigin::Mqtt);
  }
}

void CommandRouter::apply(
    const BLECommand& command, CommandOrigin origin) {
  if (command.type == BLECommandType::SetAlarm) {
    state.isAlarmSet = command.alarmEnabled;
    state.alarmHour = command.alarmHour;
    state.alarmMinute = command.alarmMinute;
    if (origin == CommandOrigin::Ble) {
      ble.notifyAlarmConfig();
      ble.notifyState();
    }
    return;
  }

  if (command.type == BLECommandType::SetLight) {
    if (!lights.applyControl(command.brightnessPercent, command.lightMode)) {
      if (origin == CommandOrigin::Ble) {
        ble.acknowledgeCommand(
            command.commandId, false, "unsupported_light_mode");
      } else {
        mqtt.acknowledgeCommand(
            command.commandId, false, "unsupported_light_mode");
      }
      return;
    }
    if (origin == CommandOrigin::Ble) {
      ble.notifyLightControl();
      ble.notifyState();
    }
  }
}
