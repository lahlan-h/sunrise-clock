# Local and Global Connections Plan

## Purpose

This document defines the setup plan for adding the project's local and global
communication paths. It is intentionally a design and class-creation plan;
connection behavior, payload publishing, and phone control are not implemented
yet.

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

1. Every input and light peripheral can expose a current, named value through a
   common device-state/telemetry model.
2. A nearby phone can connect to the ESP32 over BLE, read status, receive live
   updates, and configure values such as the alarm time.
3. The ESP32 can join Wi-Fi, recover from a dropped connection, and maintain an
   authenticated MQTT connection.
4. Meaningful peripheral data is published as MQTT telemetry, consumed by
   Telegraf, stored in InfluxDB, and displayed in Grafana.
5. MQTT commands can be received for non-time-critical remote configuration or
   control, while safety- and timing-sensitive behavior continues locally.
6. Loss of BLE, Wi-Fi, MQTT, Telegraf, InfluxDB, or Grafana does not prevent
   the clock's local sensing and alarm/light behavior.

## Current starting point

The firmware already has:

- a `Peripheral` base class and a central peripheral list in `main.cpp`;
- input and sensor classes for the rotary encoder, push button, photoresistor,
  motion sensor, and DHT sensor;
- a `LightArray` output class and `DeviceState`;
- placeholder `BLEConnection` and `WiFiConnection` classes;
- local time synchronization in `WiFiConnection`.

The connection plan should extend these existing patterns rather than make
individual peripheral classes know about BLE, MQTT, or cloud storage.

## Proposed firmware class structure

### 1. `DeviceState` and telemetry model

Update the state model first so all communication layers consume the same
source of truth.

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

### 8. `ConnectionManager` (optional coordinator)

If `main.cpp` becomes crowded, add a coordinator that owns
`BLEConnection`, `WiFiConnection`, `MQTTConnection`, `TelemetryPublisher`, and
`CommandRouter`. It should provide `begin()` and non-blocking `update()` calls.

The coordinator must not hide the timing of peripheral sampling or prevent
local control updates when a connection is unavailable.

## Communication contracts

### Local BLE path

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

1. Configure Telegraf `mqtt_consumer` for
   `sunrise-clock/+/telemetry`.
2. Parse the versioned JSON payload and map numeric readings to fields.
3. Use low-cardinality values such as `device_id`, `location`, and `state` as
   tags; keep readings, alarm values, RSSI, and brightness as fields.
4. Write to an InfluxDB bucket with a stable measurement name such as
   `sunrise_clock_telemetry`.
5. Confirm that at least one real value from each relevant input and the
   `LightArray` appears in InfluxDB.
6. Create Grafana panels for:
   - temperature and humidity;
   - ambient light and motion;
   - alarm enabled/time and current device state;
   - light brightness/output mode;
   - Wi-Fi/BLE status, RSSI, uptime, and last-seen time.
7. Add a response path only after telemetry is proven:
   Grafana/operator service -> MQTT command -> `CommandRouter` -> ESP32.

The cloud configuration must never be considered complete based only on a
successful connection. A test value must be traceable from an ESP32 reading
to MQTT, Telegraf, InfluxDB, and a Grafana panel.

## Proposed setup sequence

### Phase 1: Define contracts

- agree on `DeviceState` fields and validity rules;
- assign device identity and protocol version;
- define BLE UUIDs and characteristic formats;
- define MQTT topics and telemetry/command schemas;
- document units, ranges, and update periods.

### Phase 2: Prepare configuration

- add non-secret broker and device configuration defaults;
- extend the ignored secrets example with Wi-Fi, BLE, MQTT, and Influx/Telegraf
  deployment notes as appropriate;
- verify that credentials are not committed;
- choose whether the broker uses TLS for the target deployment.

### Phase 3: Prepare the firmware class interfaces

- add the telemetry/configuration value types;
- add read accessors or snapshots to peripheral classes;
- expand `BLEConnection` and `WiFiConnection` interfaces;
- add `MQTTConnection`, `TelemetryPublisher`, and `CommandRouter` headers;
- define ownership and update order in `main.cpp`;
- keep method bodies as setup stubs until the contracts are reviewed.

### Phase 4: Integrate local communication

- initialize BLE during firmware setup;
- expose status, telemetry, alarm configuration, and light control;
- route phone writes through `CommandRouter`;
- test read, write, notify, invalid input, reconnect, and restart behavior.

### Phase 5: Integrate global communication

- complete Wi-Fi readiness and recovery;
- add MQTT connection/reconnection/subscription handling;
- publish a real `DeviceState` snapshot;
- configure Telegraf and verify writes to InfluxDB;
- build Grafana panels from the stable measurement schema.

### Phase 6: Demonstrate and harden

- demonstrate phone alarm-time configuration over BLE;
- demonstrate live peripheral values over BLE;
- demonstrate the same meaningful values through MQTT, Telegraf, InfluxDB,
  and Grafana;
- test Wi-Fi, broker, BLE, sensor, and cloud outages;
- verify local alarm/light behavior continues during global outages;
- verify duplicate, stale, malformed, and out-of-range commands are rejected.

## Main-loop integration order

The eventual loop should remain non-blocking and follow this order:

1. update/read peripherals on their own schedules;
2. update local state and apply local alarm/light control;
3. process queued BLE commands;
4. process queued MQTT commands;
5. run BLE and MQTT client maintenance;
6. publish scheduled or changed telemetry;
7. update display/diagnostic output.

No connection failure should prevent steps 1 or 2. Avoid long delays in
connection callbacks, BLE callbacks, sensor reads, or MQTT serialization.

## Definition of done for the setup phase

The setup phase is complete when:

- the class boundaries and ownership described above are agreed and represented
  by firmware headers/stubs where required;
- every peripheral has a documented value, unit, validity rule, and update
  owner;
- BLE UUIDs, characteristic permissions, and command validation rules are
  documented;
- MQTT topics and the versioned telemetry schema are documented;
- Telegraf, InfluxDB, and Grafana configuration tasks are assigned;
- secrets remain outside source control;
- implementation and demonstration tests are listed for the next phase.
