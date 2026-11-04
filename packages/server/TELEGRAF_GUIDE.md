# Telegraf in Docker: build-it-yourself guide

This guide walks you through running Telegraf in a Docker container so that
the clock's MQTT telemetry ends up in InfluxDB Cloud. You write every file
yourself; the guide explains what each part does and how to check it works
before moving on.

```text
ESP32 (MQTTConnection::publishState)
  -> broker.emqx.io:1883, topic sunrise-clock/<device-id>/telemetry
  -> Telegraf (this container): mqtt_consumer -> json parser
  -> InfluxDB Cloud bucket, measurement sunrise_clock_telemetry
  -> Grafana (later)
```

## What you are building

When you are done, `packages/server/` will contain:

```text
packages/server/
├── docker-compose.yml      # runs the Telegraf container
├── .env                    # your Influx secrets (gitignored, never commit)
├── .env.example            # same keys with empty values (committed)
└── telegraf/
    └── telegraf.conf       # what Telegraf reads from and writes to
```

## What the firmware already sends

You need to know the payload shape to configure the parser. From
`packages/firmware/src/connectivity/MQTTConnection.cpp`:

- **Broker:** `broker.emqx.io`, port `1883`, plain MQTT, no username or
  password (see `secrets_example.h`).
- **Topic:** `sunrise-clock/<MQTT_DEVICE_ID>/telemetry`.
- **When:** on connect, after every applied command, and every 120 s.
- **Payload:** one flat JSON object, for example:

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

The keys are the `DeviceState` member names from
`packages/firmware/include/DeviceState.h`. The two exceptions are
`protocolVersion` (from `CommandRouter`) and `deviceId` (from
`MQTTConnection`), which are not part of `DeviceState`.

Three kinds of values matter for Telegraf:

| Kind | Keys | What Telegraf must do |
|---|---|---|
| Identity | `deviceId` | Make it a **tag** so you can filter by device |
| Strings | `firmwareVersion`, `lightMode`, `screen` | List them explicitly, or the JSON parser drops them |
| Numbers and booleans | everything else | Become **fields** automatically |

`temperature` and `humidity` are `null` when the DHT reading is
invalid. Telegraf skips null values, so those points simply have no
temperature field. That is fine; `temperatureValid` still records why.

## Prerequisites

1. Docker Desktop installed and running (`docker compose version` works).
2. An InfluxDB Cloud account with:
   - an organisation (note its name or ID);
   - a bucket, e.g. `sunrise-clock-bucket`;
   - an **API token with write access to that bucket only**
     (Load Data → API Tokens → Generate → Custom API Token → Write on the
     bucket). Don't use an all-access token for Telegraf.
3. The InfluxDB Cloud URL for your region, e.g.
   `https://eu-central-1-1.aws.cloud2.influxdata.com`.

## Step 1: Watch the raw MQTT traffic first

Before writing any Telegraf config, prove the data is reaching the broker.
You don't need to install anything; run the Mosquitto client in a throwaway
container:

```bash
docker run --rm -it eclipse-mosquitto:2 mosquitto_sub -h broker.emqx.io -t 'sunrise-clock/+/telemetry' -v
```

Power on the clock. Within a few seconds of it connecting you should see one
line per message: the topic, then the JSON. Leave this running in a spare
terminal; it is the quickest way to tell a firmware problem from a Telegraf
problem later.

If nothing appears, fix the firmware/Wi-Fi side first. Telegraf can't receive
what the broker never got.

## Step 2: Secrets in `.env`

Create `packages/server/.env`:

```dotenv
INFLUX_URL=https://eu-central-1-1.aws.cloud2.influxdata.com
INFLUX_ORG=your-org
INFLUX_BUCKET=sunrise-clock-bucket
INFLUX_TOKEN=paste-your-write-token-here
```

Then create `packages/server/.env.example` with the same four keys and empty
values, so Katia knows what to fill in.

Check git ignores the real file before going further:

```bash
git check-ignore -v packages/server/.env
```

It should print the matching `.gitignore` rule. If it prints nothing, **stop**
and fix `.gitignore` first.

> Note: the root `.gitignore` currently has `!.env.example'` with a stray
> trailing quote, so `.env.example` is still ignored. Remove the `'` so the
> example file can be committed.

## Step 3: Write `telegraf/telegraf.conf`

Create `packages/server/telegraf/telegraf.conf`. Build it in three sections.

### 3a. Agent

```toml
[agent]
  interval = "10s"
  flush_interval = "10s"
  omit_hostname = true
```

- `flush_interval` is how often buffered points are sent to InfluxDB.
- `omit_hostname = true` stops Telegraf adding a `host` tag containing the
  random Docker container ID, which would change on every restart.

### 3b. Input: subscribe to the clock's telemetry

```toml
[[inputs.mqtt_consumer]]
  servers = ["tcp://broker.emqx.io:1883"]
  topics = ["sunrise-clock/+/telemetry"]
  qos = 0
  client_id = "telegraf-sunrise-clock-CHANGE-ME"

  name_override = "sunrise_clock_telemetry"
  topic_tag = ""

  data_format = "json"
  tag_keys = ["deviceId"]
  json_string_fields = ["firmwareVersion", "lightMode", "screen"]
```

Why each line matters:

- `topics`: the `+` wildcard matches any device ID, so a second clock works
  with no config change.
- `client_id`: **must be unique on the public broker.** If two clients use
  the same ID, the broker kicks one off, and you will see Telegraf
  reconnecting in a loop. Replace `CHANGE-ME` with something random. Never
  reuse the ESP32's `MQTT_DEVICE_ID`.
- `name_override`: without it every point is stored under the measurement
  `mqtt_consumer`. This gives it the stable name planned in
  `docs/connections_plan.md`.
- `topic_tag = ""`: by default Telegraf tags each point with the full topic.
  `deviceId` is already in the payload, so the topic tag is a duplicate.
- `tag_keys`: turns `deviceId` into a tag. Only put low-cardinality
  identifiers here, never readings such as temperature.
- `json_string_fields`: the JSON parser ignores string values unless you list
  them. If you add a new string key to the firmware payload, add it here too.

The public broker needs no username or password, so leave those options out.
If you later move to an authenticated broker, add
`username = "${MQTT_USERNAME}"` and `password = "${MQTT_PASSWORD}"` and put
the values in `.env`.

### 3c. Outputs: stdout first, InfluxDB second

Start with only the stdout output so you can see exactly what Telegraf
produces before anything touches your bucket:

```toml
[[outputs.file]]
  files = ["stdout"]
  data_format = "influx"
```

Don't add the InfluxDB output yet. Go to Step 4, check the output, then come
back and add:

```toml
[[outputs.influxdb_v2]]
  urls = ["${INFLUX_URL}"]
  token = "${INFLUX_TOKEN}"
  organization = "${INFLUX_ORG}"
  bucket = "${INFLUX_BUCKET}"
```

Telegraf replaces `${NAME}` with the environment variable of that name when it
starts, so the token never appears in a committed file.

Once InfluxDB writes are confirmed, delete the `outputs.file` block (or keep
it while you're still debugging; it only writes to the container log).

## Step 4: Write `docker-compose.yml`

Create `packages/server/docker-compose.yml`:

```yaml
services:
  telegraf:
    image: telegraf:1.36
    container_name: sunrise-clock-telegraf
    restart: unless-stopped
    env_file: .env
    volumes:
      - ./telegraf/telegraf.conf:/etc/telegraf/telegraf.conf:ro
```

- `image`: pin a specific version so a new Telegraf release can't silently
  change behaviour. Check the tags on Docker Hub (`hub.docker.com/_/telegraf`)
  and pick the newest `1.x` you like; `1.36` is only an example.
- `env_file`: passes your `.env` values into the container as environment
  variables, which is what the `${...}` placeholders read.
- `volumes`: the official image reads `/etc/telegraf/telegraf.conf` by
  default, so mounting your file there is all the wiring needed. `:ro`
  makes it read-only inside the container.
- `restart: unless-stopped`: comes back after a crash or Docker restart, but
  stays down if you stop it yourself.

No `ports:` section is needed. Telegraf only makes outbound connections (to
the broker and to InfluxDB), so nothing has to be exposed.

## Step 5: Run it and read the output

From `packages/server/`:

```bash
docker compose up -d
```

```bash
docker compose logs -f telegraf
```

You should see something like:

```text
I! Loaded inputs: mqtt_consumer
I! Loaded outputs: file
I! [inputs.mqtt_consumer] Connected [tcp://broker.emqx.io:1883]
```

followed, each time the clock publishes, by one line of InfluxDB line
protocol:

```text
sunrise_clock_telemetry,deviceId=sunrise-clock-01 temperature=24.5,humidity=48.2,motionDetected=false,lightMode="STEADY",screen="HOME",... 1793800000000000000
```

Check that line carefully:

- [ ] The measurement is `sunrise_clock_telemetry`, not `mqtt_consumer`.
- [ ] `deviceId` is before the space (a tag), not after it (a field).
- [ ] There is no `host=` or `topic=` tag.
- [ ] `lightMode`, `screen` and `firmwareVersion` appear as quoted strings.
- [ ] Booleans such as `motionDetected` and `isLightOn` appear as `true`/`false`.

You don't have to wait two minutes for the clock. Publish a fake message
from another terminal and watch it appear in the Telegraf log:

```bash
docker run --rm eclipse-mosquitto:2 mosquitto_pub -h broker.emqx.io -t 'sunrise-clock/test-device/telemetry' -m '{"deviceId":"test-device","temperature":21.5,"motionDetected":true,"lightMode":"SUNRISE","screen":"HOME","firmwareVersion":"test"}'
```

After editing `telegraf.conf`, restart the container to reload it:

```bash
docker compose restart telegraf
```

## Step 6: Turn on InfluxDB and verify end to end

1. Add the `[[outputs.influxdb_v2]]` block from Step 3c.
2. `docker compose restart telegraf`, then watch the logs. There should be no
   `E!` lines mentioning `outputs.influxdb_v2`.
3. In InfluxDB Cloud, open **Data Explorer → Script Editor** and run:

```flux
from(bucket: "sunrise-clock-bucket")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "sunrise_clock_telemetry")
  |> filter(fn: (r) => r.deviceId == "sunrise-clock-01")
```

4. Trace one real value all the way through, as the connections plan
   requires: cover the photoresistor, note the `ambientLightPercent` in the
   `mosquitto_sub` window, then find the same value in InfluxDB.
5. Delete the test points you published in Step 5, or just ignore
   `deviceId=test-device` in Grafana.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Telegraf connects and disconnects repeatedly | `client_id` clashes with another client on the public broker | Make `client_id` unique |
| No line-protocol lines, but `mosquitto_sub` shows messages | Wrong `topics` pattern, or a JSON parse error | Compare the topic exactly; look for `E! [inputs.mqtt_consumer]` in the logs |
| Measurement is `mqtt_consumer` | `name_override` missing or misspelt | Add it under `[[inputs.mqtt_consumer]]` |
| A string value is missing | Key not in `json_string_fields` | Add the key |
| `401 Unauthorized` from InfluxDB | Wrong token, or token lacks write on this bucket | Regenerate a write token for the bucket |
| `404` / `bucket not found` | Bucket or org name mismatch | Copy both exactly from the InfluxDB UI |
| `${INFLUX_TOKEN}` appears literally in errors | `.env` not loaded | Run `docker compose` from `packages/server/`, where `.env` lives |
| Gaps in data | Telegraf missed messages while it was down | Expected with `qos = 0` on a public broker; telemetry is re-sent every 120 s |

## Done checklist

- [ ] `mosquitto_sub` shows the clock's telemetry.
- [ ] Telegraf logs show correctly shaped line protocol.
- [ ] `.env` is ignored and `.env.example` is committed.
- [ ] One real reading is traceable from the ESP32 to InfluxDB.
- [ ] Tick the matching boxes in `docs/connections_plan.md`.

Next step after this: Grafana panels on `sunrise_clock_telemetry`, as listed in
the "Telegraf, InfluxDB, and Grafana setup plan" section of
`docs/connections_plan.md`.
