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
- a substantially implemented `BLEConnection`, an incomplete
  `WiFiConnection`, an MQTT wrapper wired into `main.cpp`, and a telemetry
  publisher;
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
- [x] Copy peripheral values into `DeviceState` through
  `DeviceStateUpdater`.
- [ ] Keep connectivity classes reading the state model rather than GPIO or
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
only where needed. `main.cpp` updates the peripherals, while
`DeviceStateUpdater` copies their values into `DeviceState`; connectivity
classes read the state model, not GPIO or driver internals.

`DeviceStateUpdater` is constructed with references to the shared state,
connectivity wrappers, and sensor/input peripherals. Its non-blocking
`update()` method synchronizes Wi-Fi and BLE status, DHT readings and
validity, ambient light, motion, button, and rotary position.

The setup phase should define, for every published value:

- stable field name;
- unit and expected range;
- sampling/update period;
- DHT invalid behavior where applicable;
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
| `75c3662a-606b-417c-a9a2-3f268ef96579` | service |
| `b707bc8f-92f4-4f46-81c2-a34788fd7382` | status |
| `0a8b765b-73ac-4813-8441-dbe23e992974` | alarm configuration |
| `7292c4d1-c619-4075-8954-0535d9d1aebf` | light control |
| `3f52dc07-56d2-4141-b8fa-f7355972d74e` | device information |
| `804687da-761a-42ec-b67b-a4c635ef9f85` | command acknowledgement |

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
validation, command queuing, acknowledgements, and publish methods. It is
constructed and updated by `main.cpp`; application command routing and full
Wi-Fi recovery remain incomplete.

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
- publish simple telemetry without blocking sensing.

Use a unique device ID in the client ID and topic namespace. Keep broker
credentials in the ignored secrets/configuration path.

### 6. `TelemetryPublisher`

Status: `[x]` Implemented. `TelemetryPublisher` serializes a bounded,
versioned `DeviceState` snapshot and publishes it through `MQTTConnection`.

Create a separate publisher/serializer so MQTT transport and data modeling are
not coupled.

Proposed responsibilities:

- take a snapshot from `DeviceState`;
- serialize simple, stable JSON with meaningful sensor and actuator fields;
- publish at a scheduled interval and when important state changes;
- include DHT temperature/humidity validity only;
- include `device_id`, `location` (if used), protocol version, and state;
- publish health data such as uptime, RSSI, and connection status;
- record publish failures for diagnostics.

The firmware publishes immediately after the MQTT connection becomes
available when state changes, and otherwise every 120 seconds by default.
The interval is configured in `MQTTConnection`, not in `secrets.h`. Invalid
DHT readings are represented by `null` values with explicit validity fields.
Other peripheral values do not have separate validity fields.

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

Status: `Implemented for BLE and MQTT`. Both communication command queues are
routed through the shared application boundary. Physical-control commands and
the future alarm controller remain planned extensions.

`CommandRouter` is the application-level command boundary. It is separate
from the `Connections` interface:

| Component | Responsibility |
|---|---|
| `Connections` | Shared `begin()` and non-blocking `update()` lifecycle for communication transports |
| `CommandRouter` | Decide what an accepted command means to the clock and apply it to application state or control services |

`Connections` remains small and transport-agnostic. It is used by
`WiFiConnection`, `BLEConnection`, and `MQTTConnection` so `main.cpp` can
start and maintain each service uniformly. It must not know about alarms,
lights, audio, sensors, or command semantics.

`CommandRouter` consumes commands queued by BLE and MQTT and applies the same
application rules to both transports. It should eventually consume commands
from physical controls as well. It should not parse BLE characteristics,
manage MQTT topics, or access hardware drivers directly.

Current responsibilities:

- consume queued BLE commands;
- consume queued MQTT commands;
- apply validated alarm configuration to `DeviceState`;
- apply supported light commands through `LightArray`;
- notify the BLE client when BLE application state changes;
- return explicit rejections for unsupported light modes through the originating
  transport.

Planned responsibilities:

- consume commands from physical controls;
- extend the transport-neutral command type beyond the current
  `BLECommand` name;
- expand application-level validation beyond the range checks already
  performed by BLE and MQTT parsers;
- distinguish configuration commands from immediate control commands;
- update `DeviceState` only after all validation succeeds;
- apply alarm changes through an alarm controller once the local alarm state
  machine is implemented;
- apply light changes through `LightArray`, without accessing NeoPixel
  implementation details;
- route audio settings or playback requests through `Mp3Player` or a future
  audio controller;
- reject duplicate, stale, malformed, unsupported, or unsafe commands;
- return consistent `queued`, `applied`, `rejected`, or `failed` results to
  the originating transport.

The router is an application coordinator rather than a hardware driver.
BLE-specific behavior stays in `BLEConnection`, MQTT-specific behavior stays
in `MQTTConnection`, LED rendering stays in `LightArray`, and DFPlayer serial
communication stays in `Mp3Player`.

The current command structure is named `BLECommand` because BLE was
implemented first. MQTT now uses the same structure. Rename it to a
transport-neutral type such as `DeviceCommand` when physical controls or
additional command sources are added.

### 8. `Connections` interface

Status: `[x]` Implemented in `include/Connections.h`. The existing
`Connections` interface provides the shared `begin()` and non-blocking
`update()` lifecycle used by `WiFiConnection`, `BLEConnection`, and future
connection classes. `main.cpp` currently stores connection implementations in
the `Connections* connections[]` collection and calls them through this
interface.

`Connections` is not responsible for routing application commands. That
responsibility belongs to `CommandRouter`. No separate `ConnectionManager`
class is required. If transport coordination becomes necessary, it should be
implemented through the existing `Connections` abstraction or directly in
`main.cpp` without hiding peripheral timing or preventing local control
updates when a connection is unavailable.

## Communication contracts

### Local BLE path

Status: `Partial`. The phone can read alarm/status/device information and
receive queued-command acknowledgements. BLE alarm and light commands pass
through `CommandRouter`; MQTT uses the same router through its command topic.
Physical-control routing remains future work.

```text
Phone
  -> alarm_config write -> BLEConnection
  -> validated command queue -> CommandRouter
  -> DeviceState / local alarm and light control
  -> acknowledgement + status notification -> phone
```

The phone should be able to read the current alarm configuration and all
important current readings. Notifications should be used for changes rather
than requiring continuous polling. BLE is the primary local configuration
method; serial output may remain as a development diagnostic interface.

### Global MQTT path

Status: `Partial`. The MQTT wrapper uses the planned topic namespace, is wired
into the firmware lifecycle, and queues validated commands for
`CommandRouter`. End-to-end broker, Telegraf, InfluxDB, and Grafana validation
remains incomplete.

```text
DeviceState
  -> TelemetryPublisher
  -> MQTTConnection
  -> MQTT broker
  -> Telegraf mqtt_consumer
  -> InfluxDB measurement
  -> Grafana dashboard

MQTT command client
  -> sunrise-clock/{device_id}/command
  -> MQTTConnection
  -> CommandRouter
  -> DeviceState / LightArray
  -> sunrise-clock/{device_id}/command/ack
```

Use a stable namespace such as:

```text
sunrise-clock/{device_id}/telemetry
sunrise-clock/{device_id}/state
sunrise-clock/{device_id}/command
sunrise-clock/{device_id}/command/ack
```

The telemetry topic is for measurements and status. The command topic accepts
the versioned `set_alarm` and `set_light` commands, which are queued by
`MQTTConnection` and applied by `CommandRouter`. The acknowledgement topic
reports transport-level parsing/queue results and application-level rejections.
MQTT command handling must not be treated as a replacement for the local alarm
state machine.

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
published on important state changes and every 120 seconds by default.

### Phase 2: Prepare configuration

- [x] Add non-secret broker and device configuration defaults.
- [x] Extend the ignored secrets example with Wi-Fi, BLE, MQTT, and Influx/Telegraf
  deployment notes as appropriate;
- [x] Verify that credentials are not committed.
- [x] Choose and verify whether the broker uses TLS for the target deployment.

The current target is plain MQTT on the trusted local lab network. The
configuration retains `MQTT_USE_TLS` and `MQTT_ROOT_CERTIFICATE` so a later
deployment can enable TLS without changing the MQTT class interface. The
telemetry interval is configured through
`MQTTConnection::DEFAULT_TELEMETRY_INTERVAL_MS`, alongside the MQTT transport
configuration rather than the secrets file.
`secrets.h` is intentionally local-only and is excluded by `.gitignore`, as
confirmed during implementation. A repository-history audit is still prudent
before publishing the repository.

### Phase 3: Prepare the firmware class interfaces

- [x] Add the telemetry/configuration value types.
- [x] Add read accessors or snapshots to all required peripheral classes.
- [x] Expand `BLEConnection` and add the initial `WiFiConnection` interface.
- [x] Add the `MQTTConnection` header.
- [x] Add the `TelemetryPublisher` header and implementation.
- [x] Add the `CommandRouter` for BLE and MQTT alarm and light commands.
- [x] Add `DeviceStateUpdater` to keep peripheral synchronization out of
  `main.cpp`.
- [x] Define the connection lifecycle abstraction in `include/Connections.h`.
- [x] Define connection ownership and update order in `main.cpp`.

`DeviceStateUpdater` now owns the snapshot boundary for sensor, input, Wi-Fi
health, and `LightArray` output values. Connectivity adapters consume this
state rather than reading peripheral drivers directly.

### Phase 4: Integrate local communication

- [x] Initialize BLE during firmware setup.
- [x] Expose status, telemetry, alarm configuration, and light control
  completely.
- [x] Route phone writes through `CommandRouter`.
- [ ] Test read, write, notify, invalid input, reconnect, and restart behavior.

Phase 4 verification cases:

1. Read `status`, `alarm_config`, and `device_info` after connecting.
2. Write a valid alarm command and verify the acknowledgement, updated
   `alarm_config`, and status notification.
3. Write valid `OFF`, `STEADY`, and `SUNRISE` light commands at brightness
   values `0`, `50`, and `100`; verify the LED output and notification.
4. Write malformed JSON, unsupported protocol versions, invalid alarm ranges,
   invalid brightness, and unsupported light modes; verify rejection.
5. Disconnect and reconnect the phone; verify advertising resumes and reads
   return current runtime state.
6. Restart the ESP32 and verify configuration returns to defaults because
   Phase 4 intentionally does not persist settings.

LightBlue test payloads:

```json
{"protocol_version":1,"command_id":"alarm-001","enabled":true,"hour":6,"minute":30}
{"protocol_version":1,"command_id":"light-001","brightness_percent":50,"mode":"STEADY"}
{"protocol_version":1,"command_id":"light-002","brightness_percent":100,"mode":"SUNRISE"}
{"protocol_version":1,"command_id":"light-003","brightness_percent":0,"mode":"OFF"}
```

Write the alarm payload to `alarm_config` and light payloads to
`light_control`. Subscribe to `command acknowledgement`, `status`,
`alarm_config`, and `light_control` notifications before writing. Each valid
write should first acknowledge `accepted: true` with `message: "queued"`,
then produce the corresponding state notification after the main loop applies
the command.

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
- [ ] Demonstrate MQTT alarm and light commands through
  `CommandRouter`, including acknowledgement and rejection cases;
- [ ] Test Wi-Fi, broker, BLE, sensor, and cloud outages.
- [ ] Verify local alarm/light behavior continues during global outages.
- [ ] Verify duplicate, stale, malformed, and out-of-range commands are
  rejected.

## Main-loop integration order

Status: `Partial`. Peripheral updates, connection maintenance, state
synchronization, telemetry publishing, and BLE/MQTT command routing are now
processed in the main loop. The local alarm state machine still needs to be
added.

The eventual loop should remain non-blocking and follow this order:

1. [x] Update/read peripherals on their own schedules.
2. [ ] Update local state and apply local alarm/light control.
3. [x] Process queued BLE commands.
4. [x] Process queued MQTT commands through `CommandRouter`.
5. [x] Run BLE and MQTT client maintenance.
6. [x] Synchronize peripheral values through `DeviceStateUpdater`.
7. [x] Publish scheduled or changed telemetry through `TelemetryPublisher`.
8. [ ] Update display/diagnostic output.

No connection failure should prevent peripheral updates or state
synchronization. Avoid long delays in connection callbacks, BLE callbacks,
sensor reads, state synchronization, or MQTT serialization.

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
