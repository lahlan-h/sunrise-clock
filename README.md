<h1 align="center">Sunrise Clock</h1>

<p align="center">A sunrise alarm clock for 48033 Internet of Things</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-in%20development-yellow" alt="status"/>
</p>

## About

This is the entire codebase for **SUNRISE CLOCK**. It includes the following packages.

- **@sunrise-clock/server**: Query APIs and forward to MQTT broker
- **@sunrise-clock/firmware**: Source code flashed onto an ESP32
- **@sunrise-clock/prints**: 3D print files and instructions to construct **SUNRISE CLOCK**

## How it works

**SUNRISE CLOCK** publishes its state over MQTT, Telegraf picks it up and stores it in InfluxDB, and from there it's ready for Grafana.

![system diagram](./system-diagram.png)

## Developers!

**SUNRISE CLOCK** uses a monorepo sturcture in order to seperate key components of the project (this being the server, firmware, and prints components). You might have noticed that the linter for **@sunrise-clock/firmware** is throwing red squiggles, that's because you've got to open this project from a workspace file.

```bash
# In your terminal (make sure you are in the sunrise-clock directory) - this will open the project as a VSCode Workspace. If you are working on the firmware use the firmware folder and so on.
code sunrise-clock.code-workspace
```

For the non terminal-inclined individuals (katia). Follow these steps instead.

```md
1. Open the project in VSCode as normal
2. Click 'File', then 'Open Workspace as File'
3. Then select the 'sunrise-clock.code-workspace' file located at the root of this project directory
```

## Firmware

What you'll need before flashing **SUNRISE CLOCK**.

- **VSCode** with the **PlatformIO** extension
- A **2.4 GHz** Wi-Fi network (the ESP32 can't see 5 GHz)
- A FAT32 **micro SD card** for the DFPlayer, laid out like this

```text
/01/001.mp3 ... /01/015.mp3   # songs
/02/001.mp3 ... /02/006.mp3   # sound effects
```

The numbers at the front are what matter, anything after them is fine (e.g. `001 Help Herself.mp3`).

```bash
# 1. Move into the firmware package
cd packages/firmware

# 2. Copy secrets_example.h and fill in your Wi-Fi, BLE name and MQTT details
cp ./include/secrets_example.h ./include/secrets.h
```

`MQTT_DEVICE_ID` has to be **unique**. We're on a public broker, so it doubles as your topic and client ID, if someone else has the same one you'll kick each other off.

```bash
# 3. Plug in the ESP32, then build and flash it
pio run -t upload

# 4. Watch the serial output
pio device monitor -b 115200
```

For the non terminal-inclined individuals (katia). Follow these steps instead.

```md
1. Open the 'firmware' folder from the workspace
2. Copy 'include/secrets_example.h' to 'include/secrets.h' and fill it in
3. Plug in the ESP32
4. Click the PlatformIO arrow (→) in the bottom bar to upload
5. Click the plug icon in the bottom bar to open the serial monitor (set it to 115200)
```

## Server

The server is a **Telegraf** container. It listens to the clock's telemetry on the MQTT broker and writes it into **InfluxDB Cloud**.

```text
ESP32 → broker.emqx.io:1883 (sunrise-clock/<device-id>/telemetry) → Telegraf → InfluxDB Cloud
```

What you'll need before starting it up.

- **Docker Desktop** installed and running (`docker compose version` should work)
- An **InfluxDB Cloud** account with
  - an organisation
  - a bucket (e.g. `sunrise-clock-bucket`)
  - an API token with **write access to that bucket only** (Load Data → API Tokens → Generate → Custom API Token). Please don't use an all-access token.

### 1. Check the clock is actually talking

Before touching Telegraf, make sure the clock's messages are reaching the broker. No install needed, this runs in a throwaway container.

```bash
# Leave this running in a spare terminal
docker run --rm -it eclipse-mosquitto:2 mosquitto_sub -h broker.emqx.io -t 'sunrise-clock/+/telemetry' -v
```

Power on the clock, a message should pop up as soon as it connects (then every 2 minutes). If nothing shows up, it's a firmware or Wi-Fi problem, not a server one.

### 2. Fill in your secrets

```bash
# Move into the server package
cd packages/server

# Copy the example and fill in your InfluxDB URL, org, bucket and token
cp .env.example .env

# Make sure git is ignoring it, this should print a .gitignore rule. If it prints nothing, STOP and fix .gitignore first
git check-ignore -v .env
```

### 3. Give Telegraf a unique client ID

Open `packages/server/telegraf/telegraf.conf` and check `client_id`. Same deal as the clock, it has to be **unique on the public broker**, so if two of us run Telegraf at the same time, change one of them. Never reuse the ESP32's `MQTT_DEVICE_ID`.

### 4. Start it up

```bash
# From packages/server
docker compose up -d

# Follow the logs
docker compose logs -f telegraf
```

You should see it load and connect.

```text
I! Loaded inputs: mqtt_consumer
I! Loaded outputs: file influxdb_v2
I! [inputs.mqtt_consumer] Connected [tcp://broker.emqx.io:1883]
```

Then a line every time the clock publishes, which looks something like this.

```text
sunrise_clock_telemetry,deviceId=sunrise-clock-01 temperature=24.5,humidity=48.2,lightMode="STEADY",screen="HOME",... 1793800000000000000
```

Don't feel like waiting 2 minutes for the clock? Send a fake message and watch it come through.

```bash
docker run --rm eclipse-mosquitto:2 mosquitto_pub -h broker.emqx.io -t 'sunrise-clock/test-device/telemetry' -m '{"deviceId":"test-device","temperature":21.5,"motionDetected":true,"lightMode":"SUNRISE","screen":"HOME","firmwareVersion":"test"}'
```

### 5. Check it landed in InfluxDB

In InfluxDB Cloud, open **Data Explorer → Script Editor** and run this.

```flux
from(bucket: "sunrise-clock-bucket")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "sunrise_clock_telemetry")
```

### Handy commands

```bash
# Restart after editing telegraf.conf (it only reads it on startup)
docker compose restart telegraf

# Stop it
docker compose down
```

### Something broke?

| Problem | Fix |
|---|---|
| Telegraf keeps connecting and disconnecting | Someone else has the same `client_id`, change it |
| `mosquitto_sub` shows messages but Telegraf doesn't | Check the `topics` pattern, and look for `E! [inputs.mqtt_consumer]` in the logs |
| A text value (like `lightMode`) is missing | Add the key to `json_string_fields` in `telegraf.conf` |
| `401 Unauthorized` from InfluxDB | Wrong token, or it can't write to this bucket |
| `bucket not found` | Bucket or org name is off, copy them exactly from InfluxDB |
| `${INFLUX_TOKEN}` shows up in the errors | `.env` isn't loading, run `docker compose` from `packages/server` |

Want the full walkthrough of why everything is set up the way it is? Check out [TELEGRAF_GUIDE.md](./packages/server/TELEGRAF_GUIDE.md).
