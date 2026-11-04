#pragma once

#include <Arduino.h>
#include <Peripheral.h>

#include "DeviceState.h"

class BLECharacteristic;
class CommandRouter;

// - - - - - - - Usage - - - - - - -
// Phone-facing BLE service with three characteristics:
//   state    read + notify   compact JSON snapshot, notified when it changes
//   command  write + notify  write a command (see CommandRouter.h); the ack is notified back here
//   info     read            device name, firmware version and protocol version
//
// Keep notified JSON under ~180 bytes: notifications are cut to the negotiated
// MTU - 3 (182 bytes on iOS, 20 bytes on Android unless the app requests more).
// - - - - - - - - - - - - - - - - -
class BLEConnection : public Peripheral {
  public:
    BLEConnection(const char* deviceName, const DeviceState& state, CommandRouter& router);

    void begin() override;
    void update() override;  // Refreshes the state characteristic about once a second

    bool isConnected() const;
    void notifyState();      // Push the latest state now; call from loop() only

  private:
    class ServerCallbacks;
    class CommandCallbacks;

    const char* deviceName;
    const DeviceState& state;
    CommandRouter& router;

    BLECharacteristic* stateCharacteristic = nullptr;
    BLECharacteristic* commandCharacteristic = nullptr;
    volatile bool connected = false;  // Written by the BLE task
    uint32_t lastRefreshMs = 0;
    char lastState[192] = {};

    void handleCommand(const char* json, size_t length);  // Runs on the BLE task
};
