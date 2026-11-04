#include "CommandRouter.h"

CommandRouter::CommandRouter(
    DeviceState& state, BLEConnection& ble, LightArray& lights)
    : state(state), ble(ble), lights(lights) {}

void CommandRouter::update() {
  BLECommand command;
  while (ble.popCommand(command)) {
    apply(command);
  }
}

void CommandRouter::apply(const BLECommand& command) {
  if (command.type == BLECommandType::SetAlarm) {
    state.isAlarmSet = command.alarmEnabled;
    state.alarmHour = command.alarmHour;
    state.alarmMinute = command.alarmMinute;
    ble.notifyAlarmConfig();
    ble.notifyState();
    return;
  }

  if (command.type == BLECommandType::SetLight) {
    if (!lights.applyControl(command.brightnessPercent, command.lightMode)) {
      ble.acknowledgeCommand(command.commandId, false, "unsupported_light_mode");
      return;
    }
    ble.notifyLightControl();
    ble.notifyState();
  }
}
