#pragma once

#include "DeviceState.h"
#include "input/PushButton.h"
#include "input/RotaryEncoder.h"
#include "sensor/DHTSensor.h"
#include "sensor/MotionSensor.h"
#include "sensor/PhotoResistor.h"
#include "connectivity/BLEConnection.h"
#include "connectivity/WiFiConnection.h"
#include "light/LightArray.h"

class DeviceStateUpdater {
  public:
    DeviceStateUpdater(
        DeviceState& state,
        WiFiConnection& wifi,
        BLEConnection& ble,
        DHTSensor& dht,
        PhotoResistor& ldr,
        MotionSensor& pir,
        PushButton& button,
        RotaryEncoder& rotary,
        LightArray& lights);

    void update();

  private:
    DeviceState& state;
    WiFiConnection& wifi;
    BLEConnection& ble;
    DHTSensor& dht;
    PhotoResistor& ldr;
    MotionSensor& pir;
    PushButton& button;
    RotaryEncoder& rotary;
    LightArray& lights;
};
