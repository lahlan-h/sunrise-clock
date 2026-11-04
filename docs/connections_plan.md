# Connections

This document describes how the clock communicates. It follows the current
firmware structure: hardware classes own hardware, `DeviceState` holds the
latest snapshot, and connection classes only transport that snapshot or
commands.

## Overview

```text
Sensors and controls
        |
        v
   main.cpp
        |
        v
   DeviceState
      /   \
     /     \
   BLE     Wi-Fi -> MQTT -> Telegraf -> InfluxDB -> Grafana
```

The clock must keep working locally when either communication path is
unavailable. Connections are useful adapters, not replacements for the local
alarm, light, display, or sensor logic.

## Shared state

[`DeviceState.h`](../packages/firmware/include/DeviceState.h) is the single
source of truth for values exposed outside the device. It contains:

- alarm time and enabled state;
- temperature, humidity, ambient light, and motion;
- button and rotary input state;
- light mode, brightness, and output state;
- sound volume and display state;
- Wi-Fi, BLE, time-sync, and uptime status.

[`main.cpp`](../packages/firmware/src/main.cpp) updates peripherals first, then
copies their current values into `DeviceState` once per loop. Connection
classes read this snapshot. They do not read GPIO pins or control individual
sensors.

Invalid DHT readings are represented by `null` in JSON and have an accompanying
`*_valid` field. Other values use the simple type already present in
`DeviceState`; raw photoresistor ADC values are not part of the public
telemetry.

## Firmware lifecycle

[`Peripheral.h`](../packages/firmware/include/Peripheral.h) gives hardware and
connection classes the same small lifecycle:

```text
begin()  once during setup
update() repeatedly from loop(), without blocking
```

`main.cpp` keeps hardware and connections in separate arrays. The runtime
order is:

1. update hardware peripherals;
2. maintain Wi-Fi, BLE, and MQTT;
3. apply queued commands;
4. refresh `DeviceState`;
5. notify BLE and publish MQTT immediately when a command changed state;
6. update the local display and other local behavior.

No connection failure should prevent the first, fourth, or sixth steps.

## BLE

[`BLEConnection.h`](../packages/firmware/src/connectivity/BLEConnection.h) and
[`BLEConnection.cpp`](../packages/firmware/src/connectivity/BLEConnection.cpp)
provide one phone-facing service:

| Characteristic | Properties | Purpose |
|---|---|---|
| `state` | read, notify | Small JSON snapshot for the phone |
| `command` | write, notify | Commands in; command acknowledgements out |
| `info` | read | Device name, firmware version, and protocol version |

The service UUID is
`75c3662a-606b-417c-a9a2-3f268ef96579`. The characteristic UUIDs are defined
next to the implementation and should not be changed without updating the
phone client.

State is refreshed about once per second and notified only when its compact
JSON changes. BLE callbacks run on the Bluetooth task, so they only submit
commands and update connection status. State serialization and other work stay
in the main loop.

## Wi-Fi and MQTT

[`WiFiConnection.h`](../packages/firmware/src/connectivity/WiFiConnection.h)
handles station mode, NTP/time-zone setup, connection status, RSSI, and
non-blocking retries.

[`MQTTConnection.h`](../packages/firmware/src/connectivity/MQTTConnection.h)
uses Wi-Fi when it is available:

```text
sunrise-clock/{device_id}/telemetry
sunrise-clock/{device_id}/command
```

MQTT reconnects with bounded backoff, subscribes to the command topic after
each connection, and publishes a complete `DeviceState` snapshot:

- on connect or reconnect;
- every 120 seconds;
- immediately after an accepted command changes state.

The current implementation uses plain MQTT on the trusted lab network.
Credentials belong in the ignored local `secrets.h`; do not commit them.

## Commands

[`CommandRouter.h`](../packages/firmware/src/CommandRouter.h) is the only
command parser and application boundary. BLE and MQTT pass the same raw JSON
payload to it:

```json
{"protocol_version":1,"command_id":"alarm-001","type":"set_alarm","enabled":true,"hour":6,"minute":30}
{"protocol_version":1,"command_id":"light-001","type":"set_light","brightness_percent":50,"mode":"STEADY"}
{"protocol_version":1,"command_id":"volume-001","type":"set_volume","volume":20}
```

`CommandRouter::submit()` validates and queues a command. `update()` runs on
the main loop and applies every queued command. This keeps transport callbacks
small and makes BLE and MQTT behave consistently.

Accepted values are:

- alarm hour `0..23`, minute `0..59`;
- light brightness `0..100`, mode `OFF`, `STEADY`, or `SUNRISE`;
- volume `0..30`.

BLE receives one acknowledgement for every command. MQTT logs rejected
commands; an accepted MQTT command is visible in the next immediate telemetry
publish.

## Cloud path

The intended path is:

```text
MQTT telemetry
  -> Telegraf mqtt_consumer
  -> InfluxDB
  -> Grafana
```

The firmware side is implemented. Telegraf, InfluxDB, and Grafana still need
deployment configuration and an end-to-end test using a real sensor value.
The server package is currently only a placeholder and is not part of the
firmware communication path.

## Remaining work

- [ ] Test BLE reads, notifications, valid commands, invalid commands, and
      reconnects with a phone.
- [ ] Configure Telegraf to consume `sunrise-clock/+/telemetry`.
- [ ] Verify a real telemetry value reaches InfluxDB and appears in Grafana.
- [ ] Test Wi-Fi, broker, BLE, and cloud outages.
- [ ] Finish the local alarm/light state machine and verify it is independent
      of all connection services.
- [ ] Decide whether command persistence or duplicate-command protection is
      needed.

Until these tests are complete, connection code should remain thin. Add new
behavior to the shared state or command router first, rather than duplicating
logic in BLE, MQTT, or individual peripheral classes.
