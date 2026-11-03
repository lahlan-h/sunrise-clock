#include "DeviceStateUpdater.h"

#include <cmath>

DeviceStateUpdater::DeviceStateUpdater(
    DeviceState& state,
    WiFiConnection& wifi,
    BLEConnection& ble,
    DHTSensor& dht,
    PhotoResistor& ldr,
    MotionSensor& pir,
    PushButton& button,
    RotaryEncoder& rotary)
    : state(state),
      wifi(wifi),
      ble(ble),
      dht(dht),
      ldr(ldr),
      pir(pir),
      button(button),
      rotary(rotary) {}

void DeviceStateUpdater::update() {
  state.isWifiConnected = wifi.isConnected();
  state.temperatureC = dht.getTemperature();
  state.humidityPercent = dht.getHumidity();
  state.temperatureValid =
      !dht.isError() && std::isfinite(state.temperatureC);
  state.humidityValid =
      !dht.isError() && std::isfinite(state.humidityPercent);
  state.ambientLightPercent = ldr.getLightLevel();
  state.motionDetected = pir.isMotionDetected();
  state.buttonPressed = button.getIsPushed();
  state.rotaryPosition = rotary.getClicksSinceReset();
  state.isBluetoothConnected = ble.isConnected();
}
