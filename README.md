# PSP OBD2 Dashboard

Real-time automotive telemetry dashboard for the Sony PSP, communicating with a Honda Accord 2004 ECU via a Vgate iCar 2 WiFi OBD2 adapter (ELM327-compatible) over direct TCP/IP.

## Features

- 4 dashboard modes switchable at runtime
  - **Digital** - bar graph RPM, large speed readout, coolant/throttle/load/voltage bars
  - **Analog** - dual circular tachometer + speedometer with animated needle interpolation
  - **Diagnostics** - DTC fault code reader (OBD-II Mode 03) with live sensor strip
  - **Performance** - 0-100 km/h timer, peak RPM tracking, throttle/speed live bars
- 2 visual themes switchable at runtime without reconnect
  - **OEM Honda** - monochrome, minimal, factory cluster feel
  - **Racing Tuner** - cyan/red accent, animated gauge needles, high-contrast
- ELM327 initialization sequence with auto-protocol detection
- Exponential backoff reconnection on adapter disconnect
- Moving-average smoothing filter per PID (window = 4 samples)
- Settings persistence to Memory Stick (`ms0:/PSP/GAME/PSP-OBD2/settings.bin`)
- Debug log to Memory Stick (`ms0:/PSP/GAME/PSP-OBD2/obd2.log`)

## Hardware

| Component | Details |
|---|---|
| PSP | PSP-1000 / 2000 / 3000, firmware 3.71+ CFW |
| OBD adapter | Vgate iCar 2 WiFi, TCP port 35000, IP 192.168.0.10 |
| Vehicle | Honda Accord 2004 (OBD-II CAN / ISO 9141-2) |

## PIDs Polled

| PID | Signal | Decode |
|---|---|---|
| 010C | Engine RPM | `((A*256)+B)/4` |
| 010D | Vehicle Speed km/h | `A` |
| 0105 | Coolant Temp C | `A-40` |
| 0111 | Throttle % | `A*100/255` |
| 010F | Intake Air Temp C | `A-40` |
| 0104 | Engine Load % | `A*100/255` |
| 0142 | Control Module Voltage | `((A*256)+B)/1000` |

## Build

### Requirements

- PSPSDK toolchain (`psp-gcc`, `psp-config`, `psp-prxgen`, `pack-pbp`)
- `libmpc` (macOS: `brew install libmpc`)

### Install toolchain (macOS arm64)

```sh
curl -L https://github.com/pspdev/pspdev/releases/download/v20260501/pspdev-macos-latest-arm64.tar.gz \
     -o pspdev.tar.gz
tar -xzf pspdev.tar.gz -C ~/
export PSPDEV=~/pspdev
export PATH=$PSPDEV/bin:$PATH
brew install libmpc
```

### Compile

```sh
export PSPDEV=~/pspdev
export PATH=$PSPDEV/bin:$PATH
make
```

Output: `EBOOT.PBP` (~226KB)

### Run tests (native, no PSP required)

```sh
make -f tests/Makefile.tests
./tests/test_runner
```

## Deploy

1. Copy `EBOOT.PBP` to `ms0:/PSP/GAME/PSP-OBD2/EBOOT.PBP` on your Memory Stick
2. Configure WiFi on PSP: Settings > Network Settings > Infrastructure Mode > New Connection
   - SSID: your Vgate hotspot (default: `Vgate-xxxx`)
   - Security: WEP or open
   - Save as connection slot 1
3. Launch from PSP XMB: Game > Memory Stick > PSP OBD2 Dashboard

## Controls

| Button | Action |
|---|---|
| L / R | Cycle dashboard mode (Digital / Analog / Diagnostics / Performance) |
| Cross (X) | Cycle visual theme (OEM Honda / Racing Tuner) |
| Start | Save current theme + mode to Memory Stick |
| Home | Exit to XMB |

## Project structure

```
src/
  main.c           PSP module entry, callback thread
  app.c            App lifecycle state machine, main loop
  net/             WiFi (pspnet_apctl), TCP socket, reconnect backoff
  obd/             ELM327 init, PID scheduler, hex parser, DTC reader
  telemetry/       Vehicle state model, moving-average filter, unit conversion
  ui/              GU renderer, bitmap font, gauges, dashboard layouts, themes
  input/           PSP controller mapping
  config/          Binary settings persistence
  utils/           Logging, timing, memory helpers
include/           Header files mirroring src/ structure
tests/             Native unit tests (no PSP required)
assets/            Fonts, textures, UI skin placeholders
```
