# Product Requirements Document (PRD)
## PSP OBD2 Dashboard Application (ELM327 WiFi – Vgate iCar 2)

---

## 1. Overview

This document defines requirements for a lightweight embedded dashboard application running on PlayStation Portable (PSP) firmware (homebrew / PSPSDK-based), designed to interface with a WiFi OBD2 adapter (Vgate iCar 2 WiFi OBDII ELM327-compatible) installed in a Honda Accord 2004 ECU via standard OBD-II diagnostic port.

The system provides real-time vehicle telemetry visualization, diagnostic trouble code (DTC) reading, and performance analytics using ISO 15765-4 CAN (11-bit/29-bit) over TCP/IP WiFi socket communication.

---

## 2. Goals

- Real-time OBD-II telemetry visualization on PSP 480x272 TFT LCD
- Stable TCP socket communication with ELM327-compatible WiFi adapter
- Low-latency polling loop (<250ms target refresh rate)
- Minimal CPU overhead suitable for PSP MIPS R4000 @ 333MHz
- Modular UI rendering system (2D immediate-mode graphics)

---

## 3. Non-Goals

- No internet dependency (no HTTP/HTTPS APIs)
- No cloud sync or remote logging
- No Android/iOS dependency in final runtime (optional dev bridge only)
- No GPS navigation or map rendering

---

## 4. Hardware Environment

### Vehicle
- Honda Accord 2004 (OBD-II compliant)
- ECU protocol: ISO 9141-2 / ISO 14230-4 / ISO 15765-4 CAN (varies by engine variant)

### OBD Device
- Vgate iCar 2 WiFi OBDII
- ELM327 command set compatible
- TCP/IP server mode (typical ports: 35000 / 23 / 192.168.0.10)
- AT command interpreter supported

### Client Device
- Sony PSP (PSP-1000 / 2000 / 3000)
- WiFi IEEE 802.11b (WEP/WPA legacy support)
- PSPSDK homebrew environment
- 32MB RAM constraint

---

## 5. System Architecture

### High-Level Flow
```
[ECU] -> OBD-II Port -> Vgate iCar 2 WiFi Adapter
                                   |
                                   | TCP/IP (ELM327 protocol)
                                   v
                        PSP WiFi Client (BSD sockets)
                                   |
                                   v
                     OBD Parser + PID Manager Layer
                                   |
                                   v
                      UI Rendering Engine (GU / SDL)
```

---

## 6. Communication Protocol

### Transport Layer
- TCP socket connection
- Persistent session with keep-alive
- Optional reconnection handler

### Application Layer (ELM327)
Standard OBD-II PID queries:

| PID | Description |
|-----|------------|
| 010C | Engine RPM |
| 010D | Vehicle Speed |
| 0105 | Coolant Temperature |
| 0111 | Throttle Position |
| 010F | Intake Air Temperature |
| 0104 | Engine Load |
| 0142 | Control Module Voltage |

### Example Command Flow
```
SEND: 010C\r
RECV: 41 0C 1A F8
```

### Decoding Example
RPM = ((A * 256) + B) / 4

---

## 6.1 UI Theme System (New Requirement)

- Theme engine must support runtime switching without reconnect
- Theme profiles stored in PSP memory (flash or Memory Stick)
- Supported themes:
  - OEM Honda-style (minimal, monochrome, factory cluster)
  - Racing/Tuner style (tach-heavy, redline emphasis, animated gauges, Initial D / NFS inspired)
- Theme abstraction layer:
  - UI skin = layout + color palette + font set + gauge rendering rules
  - Decoupled from telemetry engine

---

## 7. Functional Requirements

### 7.1 Connection Manager
- WiFi SSID detection and selection
- DHCP IP acquisition or static IP fallback
- TCP socket initialization
- Reconnect strategy (exponential backoff)

### 7.2 OBD2 PID Scheduler
- Polling loop (100–500ms configurable)
- PID batching optimization
- Command queue system
- Timeout handling (500–1000ms)

### 7.3 Telemetry Engine
- Raw hex parsing
- PID mapping table
- Unit conversion layer:
  - °C, km/h, RPM, %, V
- Data smoothing (moving average filter optional)

### 7.4 UI Rendering Engine
- 480x272 framebuffer rendering
- Double buffering (VSync sync)
- Low-level GU primitives
- Font rasterization (bitmap font recommended)

### 7.5 Dashboard Modes

#### Mode A: Digital Dashboard
- RPM numeric + bar graph
- Speed digital readout
- Coolant temp warning indicator

#### Mode B: Analog Gauge Mode
- Circular tachometer
- Needle interpolation
- Redline highlight zone

#### Mode C: Diagnostics Mode
- DTC readout (Mode 03 OBD-II)
- Freeze frame data (Mode 02 optional)

#### Mode D: Performance Mode
- 0–100 km/h timer
- Acceleration graph
- Peak RPM tracking

---

## 8. Performance Requirements

- UI frame rate: 30 FPS minimum
- Telemetry refresh: 4–10 Hz per PID
- Socket latency tolerance: <200ms ideal
- Memory footprint: <8MB runtime target
- CPU usage: optimized polling, non-blocking I/O

---

## 9. Error Handling

- Lost WiFi connection → auto reconnect
- Invalid PID response → ignore + retry
- ECU timeout → fallback to cached value
- Adapter disconnect → UI warning state

---

## 10. Security Considerations

- No encryption required (LAN-only trusted network)
- Optional MAC filtering awareness
- No external internet exposure assumed

---

## 11. Development Stack

- Language: C (PSPSDK)
- Networking: BSD sockets (pspnet_inet / lwIP layer)
- Graphics: GU (sceGu*) or SDL 1.2 port
- Build system: Makefile + PSPSDK toolchain

---

## 12. Future Enhancements

- Bluetooth OBD support (if bridged externally)
- Raspberry Pi proxy telemetry server
- Replay mode (log driving sessions)
- Custom skins / themes
- Shift light indicator logic
- Gear estimation algorithm (RPM vs speed)

---

## 13. Open Questions

1. UI customization depth per theme (fonts, gauge physics, animations)?
2. Should theme switching be hot-swappable during driving or locked when moving?
3. Minimum acceptable PID refresh rate under load (affects CPU scheduling)?
4. Do you want optional CAN raw frame viewer (advanced diagnostics mode)?

## 14. System Constraint Decision

### Finalized Architecture Constraints (User Decision Applied)

- **Connectivity model:** PSP ↔ Vgate iCar 2 WiFi via direct TCP socket only
  - No phone, no Raspberry Pi, no proxy bridge layer
  - Single-hop LAN architecture

- **Theme system:** Fully switchable runtime UI skin engine
  - Multiple render profiles supported
  - Independent of telemetry and networking layers

## 15. Project Structure

### Repository Layout (PSPSDK Homebrew Project)
```
/psp-obd2-dashboard/
│
├── src/
│   ├── main.c                  # Entry point, system init
│   ├── app.c                   # App lifecycle manager
│   ├── net/
│   │   ├── wifi.c              # PSP WiFi init (pspnet_inet)
│   │   ├── socket.c            # TCP client for Vgate iCar 2
│   │   └── reconnect.c         # Exponential backoff reconnect logic
│   │
│   ├── obd/
│   │   ├── elm327.c            # ELM327 command interface
│   │   ├── pid.c               # PID definitions + scheduler
│   │   ├── parser.c            # Hex response parsing
│   │   └── diagnostics.c       # DTC + freeze frame handling
│   │
│   ├── telemetry/
│   │   ├── model.c             # Runtime vehicle state model
│   │   ├── filter.c            # Smoothing / moving average
│   │   └── units.c             # Conversion (RPM, °C, km/h)
│   │
│   ├── ui/
│   │   ├── renderer.c          # GU draw loop (sceGu)
│   │   ├── gauge.c             # Tach/speed gauge rendering
│   │   ├── dashboard.c         # Screen layouts
│   │   └── themes.c            # OEM vs tuner theme engine
│   │
│   ├── input/
│   │   └── controls.c          # PSP button handling
│   │
│   ├── config/
│   │   └── settings.c          # Persistent config (Memory Stick)
│   │
│   └── utils/
│       ├── log.c               # Debug logging
│       ├── time.c              # timers, frame sync
│       └── memory.c            # allocation helpers
│
├── assets/
│   ├── fonts/
│   ├── textures/
│   ├── ui_oem/
│   └── ui_tuner/
│
├── build/
├── Makefile
└── README.md
```

### Core Runtime Loop
```
init_system()
  -> init_wifi()
  -> connect_vgate_tcp()
  -> init_obd_session()
  -> init_ui()

while(running)
{
  poll_socket()
  send_pid_requests()
  parse_responses()
  update_telemetry_model()
  render_ui_frame()
  sleep(frame_time)
}
```

### Key Design Principles
- Non-blocking socket I/O
- Decoupled telemetry vs rendering pipeline
- Fixed timestep PID polling scheduler
- Minimal heap fragmentation (static buffers preferred)
- Theme system fully independent module

## 16. Summary Summary

This system transforms PSP into a dedicated automotive telemetry terminal using ELM327 WiFi OBD-II interface over TCP/IP, enabling real-time ECU diagnostics, performance monitoring, and retro-style dashboard visualization optimized for low-power embedded MIPS hardware.

