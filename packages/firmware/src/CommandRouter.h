#pragma once

#include "DeviceState.h"
#include "connectivity/BLEConnection.h"
#include "connectivity/MQTTConnection.h"
#include "light/LightArray.h"

enum class CommandOrigin : uint8_t {
  Ble,
  Mqtt
};

class CommandRouter {
  public:
    CommandRouter(
        DeviceState& state,
        BLEConnection& ble,
        MQTTConnection& mqtt,
        LightArray& lights);

    void update();

  private:
    DeviceState& state;
    BLEConnection& ble;
    MQTTConnection& mqtt;
    LightArray& lights;

    void apply(const BLECommand& command, CommandOrigin origin);
};
