#pragma once

#include "DeviceState.h"
#include "connectivity/BLEConnection.h"
#include "light/LightArray.h"

class CommandRouter {
  public:
    CommandRouter(DeviceState& state, BLEConnection& ble, LightArray& lights);

    void update();

  private:
    DeviceState& state;
    BLEConnection& ble;
    LightArray& lights;

    void apply(const BLECommand& command);
};
