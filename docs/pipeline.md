# Reusable IoT pipeline guide

This guide distils the progression in the weekly ESP32 labs into a reusable
pipeline for a new project. It is intentionally written around interfaces and
responsibilities rather than the specific LEDs, buttons, thermistors,
photoresistors, sonar, servo, buzzer, or audio module used in the labs.

The overall data and control path is:

```text
physical inputs
      |
      v
ESP32 sampling, filtering and state/control logic
      |
      +--> local serial, BLE or direct actuator output
      |
      +--> Wi-Fi/MQTT (or HTTPS)
                  |
                  v
            Telegraf -> InfluxDB -> Grafana
                                      |
                                      +--> MQTT command -> ESP32 response
```

## What each week contributes

The source files in `test/` show the following progression:

| Lab | Main technique | Reusable idea |
|---|---|---|
| 3 | Buttons, GPIO and LED output | Configure pins, read active-low inputs, debounce, and drive outputs. |
| 4 | Serial menu and validation | Use a human-readable local command interface and validate input before acting. |
| 5 | ADC input and PWM output | Scale a raw sensor value into a control range and expose useful telemetry. |
| 6 | Capacitive touch and servo PWM | Convert event-like input into bounded actuator steps without blocking the loop. |
| 7 | Ultrasonic input and Wi-Fi scanning | Schedule measurements, handle missing readings, and use radio information as data. |
| 8 | BLE GATT | Expose a service with readable/writable characteristics and notifications. |
| 9 | Wi-Fi and MQTT | Publish telemetry, subscribe to commands, and reconnect while preserving subscriptions. |
| 10 | InfluxDB Cloud and time-series points | Add tags and fields, synchronise time, and write measurements reliably. |
| 11 | Multi-sensor state and cloud response | Derive a state from several inputs and use remote commands to affect an alarm/actuator. |

`src/audiotest.cpp` is an additional local-output example: it uses a second
hardware serial port to control an audio module. It demonstrates that a
peripheral can be replaced without changing the telemetry pipeline.

## 1. Sensing / input

Choose the input driver for each peripheral, then convert the reading into a
named value with a known unit. Keep hardware details inside small functions so
the rest of the application does not depend on a particular sensor.

Typical setup:

```cpp
#include <Arduino.h>

constexpr uint8_t SENSOR_PIN = 34;       // replace for the new board
constexpr uint32_t SAMPLE_PERIOD_MS = 500;
uint32_t lastSampleMs = 0;
float sensorValue = 0.0f;

float readSensor() {
    const int raw = analogRead(SENSOR_PIN);
    // Replace this conversion with the calibration for the new sensor.
    return static_cast<float>(raw);
}

void setup() {
    Serial.begin(115200);
    pinMode(SENSOR_PIN, INPUT);
    analogReadResolution(12);
}

void loop() {
    const uint32_t now = millis();
    if (now - lastSampleMs >= SAMPLE_PERIOD_MS) {
        lastSampleMs = now;
        sensorValue = readSensor();
    }
}
```

The labs also use:

- `digitalRead` with `INPUT_PULLUP` for switches (pressed is usually `LOW`).
- `touchRead` with a threshold and edge detection for capacitive inputs.
- `pulseIn` for ultrasonic timing, with a timeout so a missing echo cannot
  block the application indefinitely.
- `analogReadMilliVolts` or a calibrated ADC conversion for analog sensors.
- A second `HardwareSerial` instance for peripherals such as an audio player.

For every input, document its range, unit, invalid-reading value, calibration
constants, sample period, and whether a lower or higher raw value means more
of the physical quantity.

## 2. ESP32 processing & control

The ESP32 should translate readings into application state and control outputs.
Use bounded mappings, explicit states, and non-blocking timing. The week 5
and week 6 examples map ADC/touch values to PWM or servo positions; week 11
extends this into a state machine.

```cpp
enum class DeviceState {
    Normal,
    Warning,
    Active
};

DeviceState state = DeviceState::Normal;
uint32_t stateStartedMs = 0;

void updateState(float value, uint32_t now) {
    if (value >= WARNING_LIMIT && state == DeviceState::Normal) {
        state = DeviceState::Warning;
        stateStartedMs = now;
    } else if (state == DeviceState::Warning &&
               now - stateStartedMs >= WARNING_DELAY_MS) {
        state = DeviceState::Active;
    } else if (value <= RESET_LIMIT) {
        state = DeviceState::Normal;
    }
}

void applyOutputs() {
    const bool outputOn = state == DeviceState::Active;
    digitalWrite(OUTPUT_PIN, outputOn ? HIGH : LOW);
}
```

Define `WARNING_LIMIT`, `RESET_LIMIT`, `WARNING_DELAY_MS`, and `OUTPUT_PIN`
for the new project. Use hysteresis when a noisy value could otherwise make
the output rapidly switch on and off.

For PWM outputs, keep the input and output domains explicit:

```cpp
const int duty = constrain(
    map(static_cast<int>(inputValue), INPUT_MIN, INPUT_MAX, 0, 255),
    0, 255);
ledcWrite(PWM_CHANNEL, duty);
```

The main loop should call `client.loop()` for MQTT regularly and schedule
sampling, publishing, and actuator patterns with `millis()`. Avoid long
`delay()` calls in the production loop; they reduce MQTT responsiveness and
can make button or safety events appear to be missed.

## 3. Local communication

Local communication is useful for commissioning, debugging, configuration, and
low-latency control before global connectivity is available.

### Serial

The week 4 menu demonstrates a simple text protocol:

```cpp
if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "status") {
        Serial.println("state=normal");
    } else if (command.startsWith("set ")) {
        const String value = command.substring(4);
        // Validate and apply the value before changing hardware.
    } else {
        Serial.println("error=unknown_command");
    }
}
```

Use stable names and units in diagnostic output. A line-oriented format such
as `temperature_c=24.5` is easy to inspect and can also be consumed by a
plotter during calibration.

### BLE

The week 8 alarm uses one BLE service with separate characteristics for
commands and live range data:

```cpp
BLEService *service = server->createService(SERVICE_UUID);

BLECharacteristic *command = service->createCharacteristic(
    COMMAND_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE);
command->setCallbacks(new CommandCallbacks());

BLECharacteristic *reading = service->createCharacteristic(
    READING_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY);
reading->addDescriptor(new BLE2902());
```

For another project, define a service UUID and characteristic UUIDs that
describe the data model, document whether each characteristic is read,
written, or notified, and validate every command before applying it.

## 4. Global connectivity

Wi-Fi is the transport from the ESP32 to a broker or cloud service. Keep
network credentials and tokens outside source control; use build-time
configuration, a secrets file ignored by Git, or a provisioning flow.

```cpp
#include <WiFi.h>

constexpr char WIFI_SSID[] = "YOUR_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_PASSWORD";

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(100);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("wifi=disconnected");
        return;
    }
    Serial.print("wifi_ip=");
    Serial.println(WiFi.localIP());
}
```

The week 7 scan is a different use of the radio: it measures nearby networks
and signal levels rather than joining one. Treat scans as scheduled,
potentially expensive operations and publish an explicit invalid value when
the target network is not found.

For cloud TLS, synchronise the clock with NTP before certificate validation:

```cpp
timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
```

Use a bounded retry strategy, report connection failures, and make the device
recover when Wi-Fi disappears. Do not assume that a successful initial
connection remains valid forever.

## 5. MQTT -> Telegraf -> InfluxDB

MQTT decouples the device from the database. The ESP32 publishes measurements
to a predictable topic; Telegraf subscribes to that topic and writes the
decoded values to InfluxDB. Commands can travel in the opposite direction.

### ESP32 MQTT client

The week 9 and week 11 sketches use `PubSubClient` with a `WiFiClient`:

```cpp
#include <PubSubClient.h>

WiFiClient networkClient;
PubSubClient mqtt(networkClient);

constexpr char MQTT_HOST[] = "YOUR_BROKER_HOST";
constexpr uint16_t MQTT_PORT = 1883; // use TLS and the TLS port in production
constexpr char TELEMETRY_TOPIC[] = "project/device-01/telemetry";
constexpr char COMMAND_TOPIC[] = "project/device-01/command";

void onMessage(char *topic, byte *payload, unsigned int length) {
    // Copy, parse, validate, and then apply the command.
}

void setupMqtt() {
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(onMessage);
}

void publishTelemetry(float value) {
    char payload[96];
    snprintf(payload, sizeof(payload),
             "{\"value\":%.2f,\"state\":\"%s\"}", value, stateName());
    mqtt.publish(TELEMETRY_TOPIC, payload);
}
```

Use a unique client ID, subscribe again after every reconnect, and call
`mqtt.loop()` frequently. Prefer structured JSON when a message contains
multiple values:

```json
{"temperature_c":24.5,"light_percent":63.0,"state":"NORMAL"}
```

Use a topic namespace that includes the project, device identity, and message
purpose. Keep command topics separate from telemetry topics, and authenticate
the broker. A public broker and credentials in a sketch are suitable only for
short-lived demonstrations.

### Telegraf subscriber

A minimal Telegraf configuration can subscribe to the JSON payload and map it
to InfluxDB fields:

```toml
[[inputs.mqtt_consumer]]
  servers = ["tcp://YOUR_BROKER_HOST:1883"]
  topics = ["project/+/telemetry"]
  client_id = "telegraf-project"
  username = "$MQTT_USERNAME"
  password = "$MQTT_PASSWORD"
  data_format = "json"
  tag_keys = ["device_id", "state"]

[[outputs.influxdb_v2]]
  urls = ["https://YOUR_INFLUX_HOST"]
  token = "$INFLUX_TOKEN"
  organization = "YOUR_ORG"
  bucket = "YOUR_BUCKET"
```

If the device sends a JSON object, numeric members become fields. Decide
which values are tags before deployment: low-cardinality identifiers such as
`device_id` and `location` are appropriate tags; rapidly changing values such
as temperature should remain fields. If the device publishes flat text or
line protocol instead, configure Telegraf's parser to match that format rather
than silently dropping messages.

### InfluxDB data model

Use a stable measurement name, tags for identity, and fields for readings:

```text
device_telemetry,device_id=device-01,location=lab \
temperature_c=24.5,light_percent=63.0,status="NORMAL" 1730000000000000000
```

The direct InfluxDB examples in week 10 use the same concepts through
`Point::addTag`, `Point::addField`, `validateConnection()`, and
`writePoint()`. Whether the point is written by the ESP32 or by Telegraf,
keep the schema consistent. Add timestamps from an NTP-synchronised clock or
let InfluxDB timestamp ingestion time when that is sufficient.

## 6. Grafana / response

Grafana queries InfluxDB and turns fields into panels, thresholds, and alerts.
Build panels from the stable measurement/tag/field schema rather than from
device-specific pin names.

Example Flux query for a current temperature panel:

```flux
from(bucket: "YOUR_BUCKET")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) =>
      r._measurement == "device_telemetry" and
      r._field == "temperature_c")
  |> filter(fn: (r) => r.device_id == "device-01")
```

Useful dashboard panels include:

- current values and recent trends for each important sensor;
- device connectivity or last-seen time;
- state/status as a single-stat or table panel;
- actuator output, acknowledgement, and command history;
- RSSI, uptime, error counts, and invalid-reading counts for diagnosis.

For response paths, choose one of these patterns:

1. **Grafana alert -> webhook/service -> MQTT command** for automated or
   operator-approved actions.
2. **Grafana dashboard -> operator command service -> MQTT command** when an
   explicit human action is required.
3. **ESP32 local state machine** for safety-critical or low-latency response,
   with MQTT/Grafana used for monitoring and acknowledgement.

The week 11 alarm illustrates the third pattern: local temperature thresholds
and timing decide the alarm, while MQTT carries status and a remote
acknowledgement command. Keep this local fallback for any response that must
continue working during a Wi-Fi, broker, Telegraf, InfluxDB, or Grafana outage.

## Adaptation checklist

1. List each new peripheral, its electrical interface, range, unit, and
   invalid-reading behavior.
2. Implement and test one `read...()` function per input.
3. Define the device state machine and local actuator behavior.
4. Choose local interfaces (serial, BLE, or neither) and document commands.
5. Add Wi-Fi provisioning and bounded reconnect handling.
6. Define MQTT topics and a versioned payload schema.
7. Configure Telegraf parsing, tags, and the InfluxDB bucket.
8. Add NTP time synchronisation when device timestamps or TLS are required.
9. Create Grafana panels for values, state, health, and response history.
10. Test the degraded cases: sensor failure, Wi-Fi loss, broker loss, stale
    commands, malformed payloads, and restart/reconnect recovery.

