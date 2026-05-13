# Usage Guide

## First-time setup

### 1. Configure the Vgate adapter

The Vgate iCar 2 creates a WiFi hotspot when plugged into the OBD-II port.

- Default SSID: `Vgate-xxxx` (last 4 chars of MAC)
- Default IP: `192.168.0.10`
- Default port: `35000`
- No password by default

Plug adapter into the OBD-II port under the steering column (Honda Accord 2004: under dash, driver side).

### 2. Configure PSP WiFi

On the PSP:
1. Settings > Network Settings > Infrastructure Mode
2. New Connection
3. Scan for the Vgate SSID or enter manually
4. Security: None (or WEP if you configured one)
5. IP: Automatic (DHCP)
6. Save as slot 1

### 3. Deploy EBOOT.PBP

```
Memory Stick path: ms0:/PSP/GAME/PSP-OBD2/EBOOT.PBP
```

Create the folder `PSP-OBD2` inside `ms0:/PSP/GAME/` and copy `EBOOT.PBP` into it.

### 4. Launch

PSP XMB > Game > Memory Stick > PSP OBD2 Dashboard

---

## Startup sequence

1. **WiFi connecting** - PSP connects to AP config slot 1 (Vgate hotspot)
2. **OBD connecting** - TCP handshake to `192.168.0.10:35000`, ELM327 init
3. **Running** - Live telemetry loop begins

If connection fails, the app retries with exponential backoff (1s, 2s, 4s... max 30s).

---

## Dashboard modes

Switch with **L** (previous) / **R** (next).

### Digital mode
```
+-------------------------------------------------------+
| RPM  [=============================         ] 3840    |
|                               SPEED                   |
|                                 87 km/h               |
+-------------------------------------------------------+
| COOLANT  92C  [========= ]  THROTTLE 34%  LOAD 28%   |
+-------------------------------------------------------+
| IAT: 28C    VOLTAGE: 13.8V                            |
+-------------------------------------------------------+
```

### Analog mode
```
+-------------------------------------------------------+
|  ( RPM tach    )          ( Speed tach    )           |
|  needle animated          needle animated             |
|  0----6500----8000rpm     0-----180----240km/h        |
+-------------------------------------------------------+
| COOL: 92C   LOAD: 28%   VOLT: 13.8V                  |
+-------------------------------------------------------+
```
Needle animation is smooth (lerp 0.15/frame) in Racing Tuner theme, instant in OEM Honda theme.

### Diagnostics mode
```
+-------------------------------------------------------+
| DIAGNOSTIC TROUBLE CODES                              |
|------------------------------------------------        |
| P1  P0420  Catalyst System Efficiency Below           |
| P2  P0135  O2 Sensor Heater Circuit                   |
| (up to 16 codes shown)                                |
|                                                       |
| No faults shown as green OK indicator                 |
+-------------------------------------------------------+
| LIVE: RPM 820   SPD 0                                 |
+-------------------------------------------------------+
```
DTCs are read once on connect. Re-read by disconnecting and reconnecting.

### Performance mode
```
+-------------------------------------------------------+
| PERFORMANCE                                           |
|------------------------------------------------        |
| 0-100 km/h    8.43 s    (best recorded this session)  |
|                                                       |
| PEAK RPM      6120                                    |
|                                                       |
| CURRENT SPEED [========================      ] 87km/h |
| THROTTLE      [====================          ]        |
+-------------------------------------------------------+
```
Timer starts automatically when speed < 5 km/h and throttle > 80%. Stops at 100 km/h. Best time shown.

---

## Themes

Switch with **Cross (X)**.

| Theme | Style |
|---|---|
| OEM Honda | Monochrome, minimal, low brightness - good for night driving |
| Racing Tuner | Cyan/red accent, animated needles, high contrast - tuner aesthetic |

---

## Settings

Press **Start** to save current theme and dashboard mode to Memory Stick. Settings persist across power cycles.

Settings file: `ms0:/PSP/GAME/PSP-OBD2/settings.bin`

To reset to defaults, delete the settings file.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| Stuck on "Connecting to WiFi" | Check PSP network settings slot 1 matches Vgate SSID |
| Stuck on "Connecting to OBD adapter" | Ensure Vgate is powered (engine on or ACC), IP 192.168.0.10 reachable |
| All values show 0 or ---  | ELM327 protocol detection failed; try cycling ignition |
| "NO DATA" responses | Honda Accord 2004 may need `ATSP4` (ISO 14230-4 KWP fast) - edit elm327.c |
| Coolant reads -40 C | Sensor stale/disconnected; adapter returns 0x00 byte |
| App exits immediately | Check `ms0:/PSP/GAME/PSP-OBD2/obd2.log` for error details |

---

## Log file

Debug log written to `ms0:/PSP/GAME/PSP-OBD2/obd2.log`. Open on PC after session to diagnose issues.

Format: `[timestamp_ms][LEVEL] message`

---

## Known limitations

- WiFi connection uses PSP network setting slot 1 only
- DTC descriptions not included (codes displayed raw, e.g. P0420)
- Imperial units toggle not yet wired to UI (metric only)
- Freeze frame data (OBD Mode 02) not implemented
