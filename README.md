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

**SUNRISE CLOCK**

- **TODO** Add full diagram of system ...

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
