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
    through a common device-state/telemetry model. `DeviceState` currently only
    contains alarm, connection, display, and basic light fields.
2. [ ] A nearby phone can connect to the ESP32 over BLE, read status, receive
    live updates, and configure values such as the alarm time. `BLEConnection`
    implements the transport and validation, but commands are not applied by
    the application and the documented UUIDs do not match the firmware UUIDs.
3. [ ] The ESP32 can join Wi-Fi, recover from a dropped connection, and
    maintain an authenticated MQTT connection. Wi-Fi setup and an MQTT wrapper
    exist, but recovery and MQTT wiring are incomplete.
4. [ ] Meaningful peripheral data is published as MQTT telemetry, consumed by
    Telegraf, stored in InfluxDB, and displayed in Grafana. No telemetry
    publisher or cloud configuration is present.
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
- a substantially implemented `BLEConnection`, an incomplete
  `WiFiConnection`, and an MQTT wrapper that is not wired into `main.cpp`;
- local time synchronization in `WiFiConnection`.

The connection plan should extend these existing patterns rather than make
individual peripheral classes know about BLE, MQTT, or cloud storage.

## Proposed firmware class structure

### 1. `DeviceState` and telemetry model

Update the state model first so all communication layers consume the same
source of truth.

Status: `Partial`. Alarm, time, connection, and basic light fields exist in
`DeviceState`, but sensor snapshots, device identity, firmware version,
uptime, error counters, validity flags, configuration source, and
acknowledgement state are not represented.

Proposed responsibilities:

- Store alarm configuration: enabled flag, hour, minute, and any repeat mode.
- Store current clock/time-sync status and connection health.
- Store the latest valid reading and validity/status for every input.
- Store light state such as on/off, brightness, colour/mode, and active pattern.
- Store device identity, firmware version, uptime, and error counters.
- Track the source and acknowledgement status of configuration changes where
  useful (`local_ble`, `mqtt`, or `physical_control`).

Prefer typed fields and explicit validity flags over sentinel values. The model
should be serializable by both BLE and MQTT adapters without exposing hardware
pins or driver-specific details.

Potential additions:

```text
include/DeviceTelemetry.h
include/AlarmConfig.h
include/ConnectionState.h
```

These can remain small value types if adding separate files is not justified.

### 2. Peripheral value adapters

Status: `Partial`. Several hardware classes have local accessors, and
`main.cpp` updates the peripherals, but their values are not copied into
`DeviceState` and `LightArray` has no telemetry snapshot/accessors.

- [ ] Add stable field names, units, expected ranges, sampling periods, and
  invalid/stale behavior for every value.
- [ ] Copy peripheral values into `DeviceState` from `main.cpp` (or a
  coordinator).
- [ ] Keep connectivity classes reading the state model rather than GPIO or
  driver internals.

Keep the existing hardware classes responsible for hardware access:

| Existing class | Values to expose |
|---|---|
| `PhotoResistor` | raw value and calibrated light level |
| `MotionSensor` | motion state |
| `DHTSensor` | temperature, humidity, and validity |
| `LightArray` | output state, brightness/colour/mode |
| `OledScreen` | current screen or display mode if useful for status |

Add clear read/accessor methods or a common snapshot method to these classes
only where needed. `main.cpp` should periodically update the peripherals and
copy their values into `DeviceState`; connectivity classes should read the
state model, not GPIO or driver internals.

The setup phase should define, for every value:

- stable field name;
- unit and expected range;
- sampling/update period;
- invalid/stale behavior;
- whether it is telemetry-only or can also be commanded.

### 3. `BLEConnection`

Status: `Partial`. The firmware creates a service, advertises, tracks
connections, exposes status/alarm/device-info characteristics, validates JSON
commands, queues accepted writes, and sends acknowledgements. It does not yet
apply queued commands, expose all important readings, or use the UUIDs listed
in this plan.

Expand `BLEConnection` into the local phone-facing transport.

Proposed responsibilities:

- initialize the BLE device name from configuration;
- create one versioned Sunrise Clock service;
- advertise and track connection/disconnection state;
- expose read-only status and telemetry characteristics;
- expose writable configuration/command characteristics;
- notify subscribed clients when selected values change;
- validate all writes before passing them to the application command layer;
- report malformed, out-of-range, or stale commands explicitly.

Suggested characteristics:

| Characteristic | Direction | Purpose |
|---|---|---|
| `status` | read + notify | connection state, current mode, alarm status |
| `alarm_config` | read + write + notify | enabled state and alarm hour/minute |
| `light_control` | write + notify | brightness/mode requests and acknowledgement |
| `device_info` | read | device ID, firmware version, protocol version |

The firmware uses the following versioned service and characteristic UUIDs:

| UUID | Characteristic |
|---|---|
| `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | service |
| `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | status |
| `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | alarm configuration |
| `6e400004-b5a3-f393-e0a9-e50e24dcca9e` | light control |
| `6e400005-b5a3-f393-e0a9-e50e24dcca9e` | device information |
| `6e400006-b5a3-f393-e0a9-e50e24dcca9e` | command acknowledgement |

Use compact JSON with `protocol_version: 1`. Alarm writes contain
`command_id`, `enabled`, `hour` (`0..23`), and `minute` (`0..59`). Light
writes contain `command_id`, `brightness_percent` (`0..100`), and `mode`.
Acknowledgements contain `command_id`, `accepted`, and `message`. The firmware
validates these fields and enqueues accepted commands; callbacks never apply
long-running hardware work directly.

### 4. `WiFiConnection`

Status: `Partial`. Station mode, credential use, initial connection request,
time-zone/NTP setup, and connection/time getters are implemented. Reconnect
management, IP/RSSI/failure reporting, and `update()`-based recovery are not.

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

Status: `Partial`. `MQTTConnection` exists with topic construction, Wi-Fi
gating, reconnect backoff, command subscription, payload-size/protocol/range
validation, command queuing, acknowledgements, and publish methods. It is not
constructed or updated by `main.cpp`, and it has no telemetry/state producer.

Create a dedicated MQTT wrapper, for example:

```text
packages/firmware/src/connectivity/MQTTConnection.h
packages/firmware/src/connectivity/MQTTConnection.cpp
```

Proposed responsibilities:

- configure broker address, port/TLS settings, credentials, and client ID;
- connect only when Wi-Fi is ready;
- reconnect with bounded backoff;
- call the MQTT client loop frequently from the main loop;
- resubscribe to command topics after every reconnect;
- validate topic, payload size, protocol version, and command shape;
- enqueue accepted commands for application processing;
- publish connection/health events without blocking sensing.

Use a unique device ID in the client ID and topic namespace. Keep broker
credentials in the ignored secrets/configuration path.

### 6. `TelemetryPublisher`

Status: `[x]` Implemented. `TelemetryPublisher` serializes a bounded,
versioned `DeviceState` snapshot and publishes it through `MQTTConnection`.

Create a separate publisher/serializer so MQTT transport and data modeling are
not coupled.

Proposed responsibilities:

- take a snapshot from `DeviceState`;
- serialize stable JSON with meaningful sensor and actuator fields;
- publish at a scheduled interval and when important state changes;
- publish only valid values, or include explicit validity fields;
- include `device_id`, `location` (if used), protocol version, and state;
- publish health data such as uptime, RSSI, and connection status;
- record publish failures for diagnostics.

The firmware publishes immediately after the MQTT connection becomes
available when state changes, and otherwise on a 30-second interval. Invalid
DHT readings are represented by `null` values with explicit validity fields.

The first payload should cover all meaningful project data, for example:

```json
{
  "device_id": "sunrise-clock-01",
  "temperature_c": 24.5,
  "humidity_percent": 48.2,
  "ambient_light_percent": 63.0,
  "motion": false,
  "button_pressed": false,
  "rotary_position": 12,
  "alarm_enabled": true,
  "alarm_hour": 6,
  "alarm_minute": 30,
  "light_on": true,
  "light_brightness_percent": 72,
  "screen": "HOME",
  "wifi_connected": true,
  "ble_connected": false,
  "state": "NORMAL"
}
```

### 7. `CommandRouter`

Status: `[ ]` Not implemented. BLE and MQTT currently have separate parsers
and queues; neither queue is routed through a shared application boundary.

Create one application-facing command boundary for both local BLE writes and
MQTT commands.

Proposed responsibilities:

- parse and validate an input command;
- enforce ranges such as hour `0..23`, minute `0..59`, and brightness
  `0..100`;
- distinguish configuration from immediate control;
- update `DeviceState` only after validation;
- apply changes through the relevant local control logic;
- return an acknowledgement or error to the originating transport;
- reject commands that are stale, malformed, unsupported, or unsafe.

This prevents BLE and MQTT from implementing different alarm or light rules.
The physical controls should use the same application-level setters where
practical.

### 8. `Connections` interface

Status: `[x]` Implemented in `include/Connections.h`. The existing
`Connections` interface provides the shared `begin()` and non-blocking
`update()` lifecycle used by `WiFiConnection`, `BLEConnection`, and future
connection classes. `main.cpp` currently stores connection implementations in
the `Connections* connections[]` collection and calls them through this
interface.

No separate `ConnectionManager` class is required. If coordination becomes
necessary, it should be implemented through the existing `Connections`
abstraction or directly in `main.cpp` without hiding peripheral timing or
preventing local control updates when a connection is unavailable.

## Communication contracts

### Local BLE path

Status: `Partial`. The phone can read alarm/status/device information and
receive queued-command acknowledgements, but important sensor readings and
application-side command effects are not available yet.

```text
Phone
  -> alarm_config write -> BLEConnection
  -> validated command -> CommandRouter
  -> DeviceState / local alarm control
  -> acknowledgement + status notification -> phone
```

The phone should be able to read the current alarm configuration and all
important current readings. Notifications should be used for changes rather
than requiring continuous polling. BLE is the primary local configuration
method; serial output may remain as a development diagnostic interface.

### Global MQTT path

Status: `Partial`. The MQTT wrapper uses the planned topic namespace, but no
telemetry is published and no end-to-end command handling is connected.

```text
DeviceState
  -> TelemetryPublisher
  -> MQTTConnection
  -> MQTT broker
  -> Telegraf mqtt_consumer
  -> InfluxDB measurement
  -> Grafana dashboard
```

Use a stable namespace such as:

```text
sunrise-clock/{device_id}/telemetry
sunrise-clock/{device_id}/state
sunrise-clock/{device_id}/command
sunrise-clock/{device_id}/command/ack
```

The telemetry topic is for measurements and status. The command topic is for
remote configuration/control and must not be treated as a replacement for the
local alarm state machine.

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

- [ ] Agree on `DeviceState` fields and validity rules.
- [x] Assign a device identity and protocol version.
- [ ] Reconcile and finalize BLE UUIDs and characteristic formats.
- [x] Define MQTT topics and command schema.
- [ ] Define the complete telemetry schema, units, ranges, and update periods.

### Phase 2: Prepare configuration

- [x] Add non-secret broker and device configuration defaults.
- [ ] Extend the ignored secrets example with Wi-Fi, BLE, MQTT, and Influx/Telegraf
  deployment notes as appropriate;
- [x] Verify that credentials are not committed.
- [ ] Choose and verify whether the broker uses TLS for the target deployment.

### Phase 3: Prepare the firmware class interfaces

- [ ] Add the telemetry/configuration value types.
- [ ] Add read accessors or snapshots to all required peripheral classes.
- [x] Expand `BLEConnection` and add the initial `WiFiConnection` interface.
- [x] Add the `MQTTConnection` header.
- [x] Add the `TelemetryPublisher` header and implementation. `CommandRouter`
  remains pending.
- [x] Define the connection lifecycle abstraction in `include/Connections.h`.
- [ ] Define final ownership and update order for all connections in `main.cpp`.

### Phase 4: Integrate local communication

- [x] Initialize BLE during firmware setup.
- [ ] Expose status, telemetry, alarm configuration, and light control
  completely.
- [ ] Route phone writes through `CommandRouter`.
- [ ] Test read, write, notify, invalid input, reconnect, and restart behavior.

### Phase 5: Integrate global communication

- [ ] Complete Wi-Fi readiness and recovery.
- [ ] Wire MQTT connection/reconnection/subscription handling into `main.cpp`.
- [ ] Publish a real `DeviceState` snapshot.
- [ ] Configure Telegraf and verify writes to InfluxDB.
- [ ] Build Grafana panels from the stable measurement schema.

### Phase 6: Demonstrate and harden

- [ ] Demonstrate phone alarm-time configuration over BLE.
- [ ] Demonstrate live peripheral values over BLE.
- [ ] Demonstrate the same meaningful values through MQTT, Telegraf, InfluxDB,
  and Grafana;
- [ ] Test Wi-Fi, broker, BLE, sensor, and cloud outages.
- [ ] Verify local alarm/light behavior continues during global outages.
- [ ] Verify duplicate, stale, malformed, and out-of-range commands are
  rejected.

## Main-loop integration order

Status: `Partial`. Peripheral updates and connection updates run, but command
queues, local application control, MQTT, and telemetry publishing are not
processed in this order yet.

The eventual loop should remain non-blocking and follow this order:

1. [x] Update/read peripherals on their own schedules.
2. [ ] Update local state and apply local alarm/light control.
3. [ ] Process queued BLE commands.
4. [ ] Process queued MQTT commands.
5. [ ] Run BLE and MQTT client maintenance.
6. [ ] Publish scheduled or changed telemetry.
7. [ ] Update display/diagnostic output.

No connection failure should prevent steps 1 or 2. Avoid long delays in
connection callbacks, BLE callbacks, sensor reads, or MQTT serialization.

## Definition of done for the setup phase

The setup phase is not complete. Current status:

- [ ] The class boundaries and ownership described above are agreed and
  represented by firmware headers/stubs where required.
- [ ] Every peripheral has a documented value, unit, validity rule, and update
  owner.
- [ ] BLE UUIDs, characteristic permissions, and command validation rules are
  consistent between documentation and firmware.
- [ ] MQTT topics and the versioned telemetry schema are documented and
  implemented.
- [ ] Telegraf, InfluxDB, and Grafana configuration tasks are assigned and
  verified end to end.
- [x] Secrets remain outside source control.
- [ ] Implementation and demonstration tests are listed and passing.
