# Local and Global Connections Plan

## Purpose

This document defines the setup plan for adding the project's local and global
communication paths. The checklist below is also an implementation review:

- `[x]` means implemented in the current repository.
- `[ ]` means still to be implemented.
- `Partial` notes identify work that exists but does not yet satisfy the full
  requirement.

The design follows the pipeline in [`pipeline.md`](./pipeline.md):

```text
Sensors and inputs -> ESP32 state/control -> local BLE -> phone
                                      |
                                      +-> Wi-Fi -> MQTT -> Telegraf
                                                   -> InfluxDB -> Grafana
```

The ESP32 remains responsible for reading peripherals and applying control
locally. Communication exposes that state and accepts validated configuration;
it must not replace the local alarm or light-control logic.

## Required outcomes

1. [ ] Every input and light peripheral can expose a current, named value
    through a common device-state/telemetry model. The current model includes
    calibrated ambient light, motion, DHT readings, light output, display
    state, and connection status.
2. [ ] A nearby phone can connect to the ESP32 over BLE, read status, receive
    live updates, and configure values such as the alarm time. `BLEConnection`
    implements the transport and validation, but commands are not yet applied
    by the application.
3. [ ] The ESP32 can join Wi-Fi, recover from a dropped connection, and
    maintain an authenticated MQTT connection. Wi-Fi setup and an MQTT wrapper
    exist, but recovery and MQTT wiring are incomplete.
4. [ ] Meaningful peripheral data is published as MQTT telemetry, consumed by
    Telegraf, stored in InfluxDB, and displayed in Grafana. The firmware
    telemetry publisher is present; cloud configuration and end-to-end
    verification remain incomplete.
5. [ ] MQTT commands can be received for non-time-critical remote configuration
    or control, while safety- and timing-sensitive behavior continues locally.
    MQTT command parsing and queuing exist, but there is no application command
    router or main-loop processing.
6. [ ] Loss of BLE, Wi-Fi, MQTT, Telegraf, InfluxDB, or Grafana does not
    prevent the clock's local sensing and alarm/light behavior. The current loop
    keeps updating peripherals, but the local alarm/light behavior and outage
    handling are not integrated or demonstrated.

## Current starting point

The firmware already has:

- a `Peripheral` base class and a central peripheral list in `main.cpp`;
- input and sensor classes for the rotary encoder, push button, photoresistor,
  motion sensor, and DHT sensor;
- a `LightArray` output class and `DeviceState`;
- `WiFiConnection`, `BLEConnection`, and `MQTTConnection` transports (MQTT
  also publishes telemetry), plus a `CommandRouter` that applies commands from
  both BLE and MQTT;
- local time synchronization in `WiFiConnection`.

The connection plan should extend these existing patterns rather than make
individual peripheral classes know about BLE, MQTT, or cloud storage.

## Proposed firmware class structure

### 1. `DeviceState` and telemetry model

Update the state model first so all communication layers consume the same
source of truth.

Status: `Implemented for the current communication scope`. Alarm, time,
connection, DHT validity, actuator output, device identity, firmware
version, and Wi-Fi health fields are represented in `DeviceState`. Error
counters, configuration source, and acknowledgement state remain future
additions.

Proposed responsibilities:

- Store alarm configuration: enabled flag, hour, minute, and any repeat mode.
- Store current clock/time-sync status and connection health.
- Store the latest readings. DHT temperature and humidity have explicit
  validity flags; other simple peripheral values are published directly.
- Store light state such as on/off, brightness, colour/mode, and active pattern.
- Store device identity, firmware version, uptime, and error counters.
- Track the source and acknowledgement status of configuration changes where
  useful (`local_ble`, `mqtt`, or `physical_control`).

Prefer typed fields and keep the model simple for this project. The model
should be serializable by both BLE and MQTT adapters without exposing hardware
pins or driver-specific details. Only DHT validity is part of the telemetry
contract.

Potential additions:

```text
include/DeviceTelemetry.h
include/AlarmConfig.h
include/ConnectionState.h
```

These can remain small value types if adding separate files is not justified.

### 2. Peripheral value adapters

Status: `Implemented for the current telemetry scope`. Hardware classes expose
the accessors needed by the current telemetry snapshot.

- [x] Add stable field names, units, expected ranges, sampling periods, and
  simple value behavior for the selected telemetry fields.
- [x] Copy peripheral values into `DeviceState` in `updateDeviceState()`
  (`main.cpp`).
- [x] Keep connectivity classes reading the state model rather than GPIO or
  driver internals.

Keep the existing hardware classes responsible for hardware access:

| Existing class | Values to expose |
|---|---|
| `PhotoResistor` | calibrated light level (percent); raw ADC is not published |
| `MotionSensor` | motion state |
| `DHTSensor` | temperature, humidity, and validity |
| `LightArray` | output state, brightness/colour/mode |
| `OledScreen` | current screen or display mode if useful for status |

Add clear read/accessor methods or a common snapshot method to these classes
only where needed. `main.cpp` updates the peripherals, and its
`updateDeviceState()` copies their values into `DeviceState` once per loop:
Wi-Fi and BLE status, time, DHT readings and validity, ambient light, motion,
button, rotary position, and light output. Connectivity classes read the
state model, not GPIO or driver internals.

The setup phase should define, for every published value:

- stable field name;
- unit and expected range;
- sampling/update period;
- DHT invalid behavior where applicable;
- whether it is telemetry-only or can also be commanded.

### 3. `BLEConnection`

Status: `Implemented`. One versioned service with three characteristics.
Command writes go straight to `CommandRouter::submit()`, which validates and
queues them, and the result is notified back on the same characteristic.
BLE callbacks run on the Bluetooth task (core 0), so they only set the
connected flag and submit commands. Everything else (state JSON,
notifications, the connect sound) runs on the main loop.

| Characteristic | UUID | Properties | Content |
|---|---|---|---|
| service | `75c3662a-606b-417c-a9a2-3f268ef96579` | | |
| `state` | `b707bc8f-92f4-4f46-81c2-a34788fd7382` | read + notify | compact JSON snapshot, refreshed every second and notified only when it changes |
| `command` | `fbaee8da-4ab7-4a1b-9264-438885e96408` | write + notify | write a command (see section 7); the acknowledgement is notified back here |
| `info` | `3f52dc07-56d2-4141-b8fa-f7355972d74e` | read | device name, firmware version, protocol version |

`state` example:

```json
{"alarm_enabled":true,"alarm_hour":6,"alarm_minute":30,"light_mode":"STEADY","light_brightness_percent":50,"volume":20,"temperature_c":24.5,"humidity_percent":48.2}
```

Acknowledgement examples:

```json
{"command_id":"alarm-001","accepted":true}
{"command_id":"alarm-002","accepted":false,"error":"invalid_alarm"}
```

Notifications are cut to the negotiated MTU minus 3 bytes (182 bytes on iOS,
20 bytes on Android unless the app requests a larger MTU), so `state` stays
under about 180 bytes. The full snapshot goes over MQTT.

### 4. `WiFiConnection`

Status: `Partial`. Station mode, credential use, the initial connection
request, time-zone/NTP setup, and the connection, time, and RSSI getters are
implemented. Recovery works too: the ESP32 core reconnects after most drops,
and `update()` calls `WiFi.reconnect()` every 30 seconds for the drops the core
gives up on. IP and last-failure reporting are not implemented.

Complete the existing Wi-Fi wrapper as the transport manager.

Proposed responsibilities:

- start station mode using the existing secrets/configuration mechanism;
- expose connection state, IP address, RSSI, and last failure;
- use bounded, non-blocking reconnect attempts;
- detect loss after initial connection;
- keep time synchronization separate from MQTT connection status;
- avoid logging credentials.

The class should not own MQTT topics or application commands. It should
provide the network readiness required by `MQTTConnection`.

### 5. `MQTTConnection`

Status: `Implemented`. `MQTTConnection` builds its topics from the device
ID and only connects while Wi-Fi is up. It reconnects with backoff (1 s,
doubling to 60 s) and resubscribes after every reconnect. Command payloads go
to `CommandRouter::submit()`, and telemetry is published as described in
section 6. It uses plain MQTT over `WiFiClient`; TLS was removed for the lab
network.

Create a dedicated MQTT wrapper, for example:

```text
packages/firmware/src/connectivity/MQTTConnection.h
packages/firmware/src/connectivity/MQTTConnection.cpp
```

Proposed responsibilities:

- configure broker address, port, credentials, and client ID (the device ID);
- connect only when Wi-Fi is ready;
- reconnect with bounded backoff;
- call the MQTT client loop frequently from the main loop;
- resubscribe to command topics after every reconnect;
- check the topic and pass command payloads to `CommandRouter`, which
  validates and queues them;
- publish simple telemetry without blocking sensing.

Use a unique device ID in the client ID and topic namespace. Keep broker
credentials in the ignored secrets/configuration path.

### 6. Telemetry

Status: `Implemented` in `MQTTConnection::publishState()`. The separate
`TelemetryPublisher` class was folded in: serializing `DeviceState` is one
ArduinoJson function, and the schedule is three lines in
`MQTTConnection::update()`.

Telemetry is published on every (re)connect, every 120 seconds
(`TELEMETRY_INTERVAL_MS` in `MQTTConnection.cpp`), and straight after a
command changes state (`main.cpp` calls `publishState()`). It does not publish
on every sensor change. RSSI and the light level change almost every loop,
and diffing the payload used to flood the broker. If motion events need to
reach Grafana between intervals, call `mqtt.publishState()` from `loop()` when
motion starts.

Readings are rounded to one decimal place. Invalid DHT readings are `null`
and have explicit validity fields; other values have none. Keys match the
`DeviceState` member names, except `protocolVersion` and `deviceId`, which
come from `CommandRouter` and `MQTTConnection`. Payload:

```json
{
  "protocolVersion": 1,
  "deviceId": "sunrise-clock-01",
  "firmwareVersion": "1.0.0",
  "temperature": 24.5,
  "temperatureValid": true,
  "humidity": 48.2,
  "humidityValid": true,
  "ambientLightPercent": 63,
  "motionDetected": false,
  "rotaryButtonPressed": false,
  "backButtonPressed": false,
  "alarmButtonPressed": false,
  "rotaryPositionSinceLastReset": 12,
  "isAlarmSet": true,
  "alarmHour": 6,
  "alarmMinute": 30,
  "isLightOn": true,
  "lightBrightnessPercent": 72,
  "lightMode": "STEADY",
  "isSoundOn": true,
  "volume": 20,
  "screen": "HOME",
  "isWifiConnected": true,
  "wifiRssi": -58,
  "isBluetoothConnected": false,
  "isTimeSynced": true,
  "upTimeMs": 123456
}
```

### 7. `CommandRouter`

Status: `Implemented for BLE and MQTT`. Physical-control commands and the
future alarm controller remain planned extensions.

`CommandRouter` is the single entry point for commands. Transports are thin
pipes: they hand the raw JSON to `submit()` and never parse it themselves.

- `submit(json, length)` parses the JSON with ArduinoJson, validates it, and
  queues it on a 4-slot FreeRTOS queue. It is safe to call from any task,
  which matters because BLE callbacks run on a different core from `loop()`.
  It returns the `command_id` plus an error code, or no error if the command
  was queued. BLE uses this to acknowledge immediately; MQTT logs rejections.
- `update()` runs from `loop()` and applies every queued command. It returns
  `true` if anything was applied, so `main.cpp` can push the new state to BLE
  and MQTT straight away.

Command JSON, the same for every transport:

```json
{"protocol_version":1,"command_id":"alarm-001","type":"set_alarm","enabled":true,"hour":6,"minute":30}
{"protocol_version":1,"command_id":"light-001","type":"set_light","brightness_percent":50,"mode":"STEADY"}
{"protocol_version":1,"command_id":"volume-001","type":"set_volume","volume":20}
```

`hour` is `0..23`, `minute` is `0..59`, `brightness_percent` is `0..100`,
`mode` is `OFF`, `STEADY`, or `SUNRISE`, and `volume` is `0..30` (DFPlayer
steps; `0` mutes). Everything is validated before the command is queued, so
each command gets exactly one acknowledgement and applying it cannot fail.

Error codes:

- `payload_too_large`: over 256 bytes;
- `invalid_json`;
- `unsupported_protocol`;
- `invalid_command_id`: missing, or longer than 32 characters;
- `unsupported_command`;
- `invalid_alarm`;
- `invalid_light`;
- `invalid_volume`;
- `queue_full`;
- `not_ready`: `submit()` was called before `begin()`.

Planned extensions:

- consume commands from physical controls;
- apply alarm changes through an alarm controller once the local alarm state
  machine exists;
- route playback requests (songs, effects) through `Mp3Player`;
- reject duplicate or stale commands if that becomes necessary.

The router is an application coordinator rather than a hardware driver. LED
rendering stays in `LightArray`, and DFPlayer serial communication stays in
`Mp3Player`.

### 8. Lifecycle interface

`WiFiConnection`, `BLEConnection`, and `MQTTConnection` implement the same
`Peripheral` interface as the hardware classes: `begin()` plus a non-blocking
`update()`. `main.cpp` keeps them in their own `connections[]` array so the
start order stays explicit. The old `Connections` interface was identical, so
it was removed.

## Communication contracts

### Local BLE path

Status: `Partial`. The phone can read `state` and `info`, write commands, and
receive acknowledgements and state notifications. BLE and MQTT commands go
through the same `CommandRouter`. Physical-control routing remains future
work.

```text
Phone
  -> command write -> BLEConnection (BLE task)
  -> CommandRouter::submit(): validate + queue
  -> acknowledgement notify -> phone
loop()
  -> CommandRouter::update() -> DeviceState / LightArray
  -> state notify -> phone
```

The phone can read the alarm, light, and temperature/humidity values.
Changes arrive as notifications, so the phone doesn't need to poll. BLE is the
primary local configuration method; serial output may remain as a
development diagnostic interface.

### Global MQTT path

Status: `Partial`. The MQTT wrapper uses the planned topic namespace, is wired
into the firmware lifecycle, and passes commands to `CommandRouter`.
End-to-end broker, Telegraf, InfluxDB, and Grafana validation remains
incomplete.

```text
DeviceState
  -> MQTTConnection::publishState()
  -> MQTT broker
  -> Telegraf mqtt_consumer
  -> InfluxDB measurement
  -> Grafana dashboard

MQTT command client
  -> sunrise-clock/{device_id}/command
  -> MQTTConnection
  -> CommandRouter::submit(): validate + queue
  -> CommandRouter::update() -> DeviceState / LightArray
```

Use a stable namespace such as:

```text
sunrise-clock/{device_id}/telemetry
sunrise-clock/{device_id}/command
```

The telemetry topic is for measurements and status. The command topic accepts
the same versioned `set_alarm` and `set_light` JSON as BLE, which
`CommandRouter` validates, queues, and applies. MQTT command handling must not
be treated as a replacement for the local alarm state machine. MQTT has no
acknowledgement topic: rejections are logged to Serial, and an applied command
triggers an immediate telemetry publish.

## Telegraf, InfluxDB, and Grafana setup plan

These are configuration deliverables for the implementation phase:

- [ ] Configure Telegraf `mqtt_consumer` for
   `sunrise-clock/+/telemetry`.
- [ ] Parse the versioned JSON payload and map numeric readings to fields.
- [ ] Use low-cardinality values such as `device_id`, `location`, and `state` as
   tags; keep readings, alarm values, RSSI, and brightness as fields.
- [ ] Write to an InfluxDB bucket with a stable measurement name such as
   `sunrise_clock_telemetry`.
- [ ] Confirm that at least one real value from each relevant input and the
   `LightArray` appears in InfluxDB.
- [ ] Create Grafana panels for:
   - temperature and humidity;
   - ambient light and motion;
   - alarm enabled/time and current device state;
   - light brightness/output mode;
   - Wi-Fi/BLE status, RSSI, uptime, and last-seen time.
- [ ] Add a response path only after telemetry is proven:
   Grafana/operator service -> MQTT command -> `CommandRouter` -> ESP32.

The cloud configuration must never be considered complete based only on a
successful connection. A test value must be traceable from an ESP32 reading
to MQTT, Telegraf, InfluxDB, and a Grafana panel.

## Proposed setup sequence

### Phase 1: Define contracts

- [x] Agree on `DeviceState` fields and validity rules.
- [x] Assign a device identity and protocol version.
- [x] Reconcile and finalize BLE UUIDs and characteristic formats.
- [x] Define MQTT topics and command schema.
- [x] Define the complete telemetry schema, units, ranges, and update periods.

The agreed contract uses the UUIDs and protocol version already implemented by
`BLEConnection`. MQTT telemetry is intentionally simple: it includes
temperature and humidity with DHT-only validity fields, calibrated ambient
light percentage, motion state, light output state/brightness/mode, display
state, and relevant alarm and connection status. Raw photoresistor ADC values
and validity fields for non-DHT peripherals are not published. Telemetry is
published on connect, after each applied command, and every 120 seconds.

### Phase 2: Prepare configuration

- [x] Add non-secret broker and device configuration defaults.
- [x] Extend the ignored secrets example with Wi-Fi, BLE, MQTT, and Influx/Telegraf
  deployment notes as appropriate;
- [x] Verify that credentials are not committed.
- [x] Choose and verify whether the broker uses TLS for the target deployment.

The firmware uses plain MQTT (no TLS) against the public broker
`broker.emqx.io:1883`, with no username or password. Anyone can read and
publish on a public broker, so `MQTT_DEVICE_ID` must be unique and hard to
guess: it is both the topic namespace and the MQTT client ID, and a duplicate
client ID makes the broker disconnect one of the two clients. TLS-only brokers
such as EMQX Cloud Serverless (port 8883) will not work without bringing back
`WiFiClientSecure`. The telemetry interval is `TELEMETRY_INTERVAL_MS`
in `MQTTConnection.cpp`, not the secrets file.
`secrets.h` is intentionally local-only and is excluded by `.gitignore`, as
confirmed during implementation. A repository-history audit is still prudent
before publishing the repository.

### Phase 3: Prepare the firmware class interfaces

- [x] Add the telemetry/configuration value types.
- [x] Add read accessors or snapshots to all required peripheral classes.
- [x] Expand `BLEConnection` and add the initial `WiFiConnection` interface.
- [x] Add the `MQTTConnection` header.
- [x] Publish telemetry from `MQTTConnection` (the separate
  `TelemetryPublisher` was folded in).
- [x] Add the `CommandRouter` for BLE and MQTT alarm and light commands.
- [x] Copy peripheral values into `DeviceState` in `updateDeviceState()`.
- [x] Use the `Peripheral` lifecycle for connections too.
- [x] Define connection ownership and update order in `main.cpp`.

`updateDeviceState()` in `main.cpp` owns the snapshot boundary for sensor,
input, Wi-Fi health, and `LightArray` output values. Connectivity adapters
read this state rather than reading peripheral drivers directly.

### Phase 4: Integrate local communication

- [x] Initialize BLE during firmware setup.
- [x] Expose state, commands, and device info.
- [x] Route phone writes through `CommandRouter`.
- [ ] Test read, write, notify, invalid input, reconnect, and restart behavior.

Phase 4 verification cases:

1. Read `state` and `info` after connecting, then subscribe to `state` and
   `command` notifications.
2. Write a valid alarm command to `command`. Verify one acknowledgement
   (`accepted: true`) and a `state` notification showing the new alarm.
3. Write valid `OFF`, `STEADY`, and `SUNRISE` light commands at brightness
   values `0`, `50`, and `100`; verify the LED output and notification.
4. Write `set_volume` commands at `0`, `15`, and `30`; verify the speaker
   volume and the `state` notification.
5. Write malformed JSON, an unsupported protocol version, invalid alarm
   ranges, invalid brightness, an unsupported light mode, and a volume above
   `30`. Verify a single `accepted: false` acknowledgement with the matching
   error each time.
6. Disconnect and reconnect the phone; verify advertising resumes and reads
   return current runtime state.
7. Restart the ESP32 and verify configuration returns to defaults because
   Phase 4 intentionally does not persist settings.

LightBlue test payloads (write each one to `command`):

```json
{"protocol_version":1,"command_id":"alarm-001","type":"set_alarm","enabled":true,"hour":6,"minute":30}
{"protocol_version":1,"command_id":"light-001","type":"set_light","brightness_percent":50,"mode":"STEADY"}
{"protocol_version":1,"command_id":"light-002","type":"set_light","brightness_percent":100,"mode":"SUNRISE"}
{"protocol_version":1,"command_id":"light-003","type":"set_light","brightness_percent":0,"mode":"OFF"}
{"protocol_version":1,"command_id":"volume-001","type":"set_volume","volume":20}
{"protocol_version":1,"command_id":"bad-001","type":"set_alarm","enabled":true,"hour":24,"minute":0}
```

Subscribe to `command` and `state` before writing. Each write produces exactly
one acknowledgement on `command`. A valid write is followed by a `state`
notification once the main loop applies it. The same payloads work on the
MQTT command topic.

### Phase 5: Integrate global communication

- [ ] Complete Wi-Fi readiness and recovery.
- [x] Wire MQTT connection/reconnection/subscription handling into `main.cpp`.
- [x] Route validated MQTT alarm and light commands through `CommandRouter`.
- [x] Publish a real `DeviceState` snapshot.
- [ ] Configure Telegraf and verify writes to InfluxDB.
- [ ] Build Grafana panels from the stable measurement schema.

### Phase 6: Demonstrate and harden

- [ ] Demonstrate phone alarm-time configuration over BLE.
- [ ] Demonstrate live peripheral values over BLE.
- [ ] Demonstrate the same meaningful values through MQTT, Telegraf, InfluxDB,
  and Grafana;
- [ ] Demonstrate MQTT alarm and light commands through `CommandRouter`,
  including valid and rejected command cases;
- [ ] Test Wi-Fi, broker, BLE, sensor, and cloud outages.
- [ ] Verify local alarm/light behavior continues during global outages.
- [ ] Verify duplicate, stale, malformed, and out-of-range commands are
  rejected.

## Main-loop integration order

Status: `Partial`. The local alarm state machine still needs to be added.

The loop stays non-blocking and runs in this order:

1. [x] Update/read peripherals on their own schedules.
2. [x] Run Wi-Fi, BLE, and MQTT maintenance (MQTT's client loop may submit
   commands).
3. [x] Apply queued BLE and MQTT commands with `CommandRouter::update()`.
4. [x] Copy peripheral and connection values into `DeviceState` with
   `updateDeviceState()`.
5. [x] Play the BLE connect sound when a phone connects.
6. [x] If a command was applied, notify BLE `state` and publish MQTT telemetry
   straight away.
7. [ ] Apply local alarm/light control.
8. [ ] Update display/diagnostic output (only the home screen so far).

No connection failure should prevent peripheral updates or state
synchronization. Avoid long delays in connection callbacks, BLE callbacks,
sensor reads, state synchronization, or MQTT serialization. One known stall:
while the broker is unreachable, each MQTT connect attempt can block the loop
for up to about 3 seconds (the `WiFiClient` connect timeout). The backoff
limits this to about once a minute.

## Definition of done for the setup phase

The setup phase is not complete. Current status:

- [ ] The class boundaries and ownership described above are agreed and
  represented by firmware headers/stubs where required.
- [ ] Every peripheral has a documented value, unit, update owner, and simple
  telemetry behavior; only DHT readings require validity fields.
- [ ] BLE UUIDs, characteristic permissions, and command validation rules are
  consistent between documentation and firmware.
- [ ] MQTT topics and the versioned telemetry schema are documented and
  implemented.
- [ ] Telegraf, InfluxDB, and Grafana configuration tasks are assigned and
  verified end to end.
- [x] Secrets remain outside source control.
- [ ] Implementation and demonstration tests are listed and passing.
