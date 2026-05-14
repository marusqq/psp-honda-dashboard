#!/usr/bin/env python3
"""
ELM327 WiFi adapter mock -- simulates Vgate iCar 2 WIFI OBDII ELM327

Listens on the same ports/IPs the PSP app probes:
  0.0.0.0:35000  (Vgate iCar 2 default)
  0.0.0.0:3000   (alternate firmware)
  0.0.0.0:23     (telnet-style)

Responds to all 27 PIDs the dashboard polls, with animated engine values.

Usage:
  python3 tools/elm327_mock.py              # listen on 35000 + 3000 + 23
  python3 tools/elm327_mock.py --port 35000 # single port only
  python3 tools/elm327_mock.py --test       # run local self-test then exit

Networking (PSP supports WEP and WPA-PSK/TKIP only -- NOT WPA2-AES):

  Option A -- Android phone hotspot (easiest):
    Android Settings > Hotspot > Security > WPA2 PSK (some phones: "WPA/WPA2")
    Set encryption to TKIP or keep WPA2 -- most Androids negotiate TKIP for old clients.
    Connect both Mac and PSP to the phone hotspot.
    Find Mac IP on that network: ipconfig getifaddr en0
    Add Mac IP as first entry in elm327.c g_probe_candidates[], rebuild.
    PSP probes that IP:35000 and hits this server.

  Option B -- old router set to WPA-TKIP:
    Same idea: Mac runs server, PSP joins same network, add Mac IP to probe list.

  Option C -- Vgate adapter's own network:
    PSP connects to real Vgate hotspot (V-LINK, WEP/WPA-TKIP).
    Mac also connects to V-LINK hotspot -- Mac gets 192.168.0.x IP.
    Add Mac's 192.168.0.x IP as first probe candidate (before 192.168.0.10).
    Probe finds mock first; real adapter still at 192.168.0.10.
    Find Mac IP on V-LINK: ipconfig getifaddr en0 (or en1 for WiFi).

  After connecting: run server without sudo (ports 35000 and 3000 only):
    python3 tools/elm327_mock.py --port 35000 --port 3000
"""

import socket
import threading
import time
import math
import argparse
import sys

# ---------------------------------------------------------------------------
# Simulated engine state -- all values animated via sinusoids
# ---------------------------------------------------------------------------

class Engine:
    def __init__(self):
        self._t0 = time.time()

    def _t(self):
        return time.time() - self._t0

    # Encode helpers
    @staticmethod
    def _pct(pct):
        return max(0, min(255, round(pct * 255 / 100)))

    @staticmethod
    def _temp(c):
        return max(0, min(255, round(c + 40)))

    @staticmethod
    def _u16(v):
        v = max(0, min(65535, round(v)))
        return (v >> 8) & 0xFF, v & 0xFF

    # --------------- raw encoded byte(s) per PID -------------------------

    def rpm(self):
        # 900..4500 rpm, slow oscillation
        r = 900 + 3600 * (0.5 + 0.5 * math.sin(self._t() * 0.35))
        v = round(r * 4)
        return (v >> 8) & 0xFF, v & 0xFF

    def speed(self):
        # 0..120 km/h
        s = max(0, 60 + 55 * math.sin(self._t() * 0.18))
        return round(s), 0x00

    def coolant(self):
        return self._temp(87 + 4 * math.sin(self._t() * 0.04)), 0x00

    def throttle(self):
        pct = max(0, min(100, 18 + 22 * (0.5 + 0.5 * math.sin(self._t() * 0.35))))
        return self._pct(pct), 0x00

    def iat(self):
        return self._temp(28 + 2 * math.sin(self._t() * 0.02)), 0x00

    def engine_load(self):
        pct = max(5, min(95, 28 + 24 * (0.5 + 0.5 * math.sin(self._t() * 0.35))))
        return self._pct(pct), 0x00

    def voltage(self):
        # 0142: decode = (a*256+b) / 1000.0  -> 13.8V = 13800
        v = round(13800 + 200 * math.sin(self._t() * 0.1))
        return (v >> 8) & 0xFF, v & 0xFF

    def stft(self):
        # decode = (a/128 - 1) * 100  -> +3% = (1.03)*128 = 131.84 -> 132
        pct = 3 + 2 * math.sin(self._t() * 2.3)
        a = max(0, min(255, round((pct / 100 + 1) * 128)))
        return a, 0x00

    def ltft(self):
        a = max(0, min(255, round((-0.02 + 1) * 128)))  # -2%
        return a, 0x00

    def timing_adv(self):
        # decode = a/2 - 64  -> 16 deg = (16+64)*2 = 160
        deg = 14 + 6 * math.sin(self._t() * 0.3)
        a = max(0, min(255, round((deg + 64) * 2)))
        return a, 0x00

    def runtime(self):
        return self._u16(self._t())

    def fuel_level(self):
        return self._pct(75), 0x00

    def ambient(self):
        return self._temp(20), 0x00

    def map_kpa(self):
        kpa = 45 + 45 * (0.5 + 0.5 * math.sin(self._t() * 0.35))
        return max(0, min(255, round(kpa))), 0x00

    def maf(self):
        # decode = (a*256+b) / 100.0 -> 4.5 g/s = 450
        gs = 1.5 + 5 * (0.5 + 0.5 * math.sin(self._t() * 0.35))
        v = round(gs * 100)
        return (v >> 8) & 0xFF, v & 0xFF

    def o2_b1s1(self):
        # decode = a / 200.0 -> oscillates 0.1..0.8V (lambda oscillation)
        v = 0.45 + 0.35 * math.sin(self._t() * 3.1)
        return max(0, min(255, round(v * 200))), 0x00

    def o2_b1s2(self):
        # post-cat stays narrow near stoich
        return max(0, min(255, round(0.65 * 200))), 0x00

    def baro(self):
        return 101, 0x00

    def rel_throttle(self):
        return self.throttle()  # same signal

    def accel_pos(self):
        return self.throttle()

    def oil_temp(self):
        return self._temp(90 + 3 * math.sin(self._t() * 0.03)), 0x00

    def fuel_rate(self):
        # decode = (a*256+b) / 20.0 -> 2.5 L/h = 50
        lh = 0.8 + 3.5 * (0.5 + 0.5 * math.sin(self._t() * 0.35))
        v = round(lh * 20)
        return (v >> 8) & 0xFF, v & 0xFF

    def ethanol(self):
        return self._pct(0), 0x00  # petrol

    def mil_time(self):
        return self._u16(0)

    def clr_time(self):
        return self._u16(5000)

    def mil_dist(self):
        return self._u16(0)

    def clr_dist(self):
        return self._u16(15000)


engine = Engine()

# ---------------------------------------------------------------------------
# ELM327 protocol
# ---------------------------------------------------------------------------

def _hex(*bytes_):
    return "".join(f"{b:02X}" for b in bytes_)


# All PIDs the dashboard polls.  Key = command string (uppercase, no spaces).
def build_pid_response(cmd: str):
    e = engine
    responses = {
        "010C": _hex(0x41, 0x0C, *e.rpm()),
        "010D": _hex(0x41, 0x0D, *e.speed()),
        "0105": _hex(0x41, 0x05, *e.coolant()),
        "0111": _hex(0x41, 0x11, *e.throttle()),
        "010F": _hex(0x41, 0x0F, *e.iat()),
        "0104": _hex(0x41, 0x04, *e.engine_load()),
        "0142": _hex(0x41, 0x42, *e.voltage()),
        "0106": _hex(0x41, 0x06, *e.stft()),
        "0107": _hex(0x41, 0x07, *e.ltft()),
        "010E": _hex(0x41, 0x0E, *e.timing_adv()),
        "011F": _hex(0x41, 0x1F, *e.runtime()),
        "012F": _hex(0x41, 0x2F, *e.fuel_level()),
        "0146": _hex(0x41, 0x46, *e.ambient()),
        "010B": _hex(0x41, 0x0B, *e.map_kpa()),
        "0110": _hex(0x41, 0x10, *e.maf()),
        "0114": _hex(0x41, 0x14, *e.o2_b1s1()),
        "0115": _hex(0x41, 0x15, *e.o2_b1s2()),
        "0133": _hex(0x41, 0x33, *e.baro()),
        "0145": _hex(0x41, 0x45, *e.rel_throttle()),
        "0149": _hex(0x41, 0x49, *e.accel_pos()),
        "015C": _hex(0x41, 0x5C, *e.oil_temp()),
        "015E": _hex(0x41, 0x5E, *e.fuel_rate()),
        "0152": _hex(0x41, 0x52, *e.ethanol()),
        "014D": _hex(0x41, 0x4D, *e.mil_time()),
        "014E": _hex(0x41, 0x4E, *e.clr_time()),
        "0121": _hex(0x41, 0x21, *e.mil_dist()),
        "0131": _hex(0x41, 0x31, *e.clr_dist()),
        # Mode 03 DTCs -- no fault codes
        "03":   "4300",
        # Mode 01 supported PID bitmasks -- claim all supported
        "0100": "410000BE3FA813",
        "0120": "41200080014001",
        "0140": "414000400000",
        "0160": "41600000000000",
    }
    return responses.get(cmd)


def handle_at(cmd: str, cs: dict) -> str:
    u = cmd.upper().strip()
    if u in ("ATZ", "ATI", "AT@1"):
        cs['echo'] = True  # ATZ resets echo on
        return "ELM327 v2.1"
    if u.startswith("ATE"):
        cs['echo'] = (u[3:] != "0") if len(u) > 3 else True
        return "OK"
    if u.startswith("ATS") and len(u) > 3 and u[3].isdigit():
        cs['spaces'] = (u[3] != "0")
        return "OK"
    if u in ("ATL0", "ATL1", "ATH0", "ATH1", "ATAT0", "ATAT1", "ATAT2",
             "ATPC", "ATAR", "ATWS", "ATMA", "ATAL", "ATCAF0", "ATCAF1",
             "ATDESC", "ATBD"):
        return "OK"
    if u.startswith("ATSP") or u.startswith("ATST") or u.startswith("ATAT"):
        return "OK"
    if u == "ATDP":
        return "AUTO, ISO 15765-4 (CAN 11/500)"
    if u == "ATDPN":
        return "A6"
    if u == "ATIGN":
        return "ON"
    if u == "ATRV":
        return "13.8V"
    return "?"


def process_command(raw: str, cs: dict) -> str:
    cmd = raw.strip()
    if not cmd:
        return ""
    upper = cmd.upper()

    if upper.startswith("AT") or upper.startswith("AT "):
        resp_body = handle_at(cmd, cs)
    else:
        resp_body = build_pid_response(upper)
        if resp_body is None:
            resp_body = "NO DATA"

    # ELM327 response: [echo\r] body\r\n>
    out = ""
    if cs.get('echo', True):
        out += cmd + "\r"
    out += resp_body + "\r\n>"
    return out


# ---------------------------------------------------------------------------
# TCP server
# ---------------------------------------------------------------------------

def handle_client(conn: socket.socket, addr, port: int):
    print(f"[+] {addr[0]}:{addr[1]}  (port {port})")
    cs = {'echo': True, 'spaces': True}
    buf = b""
    try:
        conn.settimeout(60)
        while True:
            try:
                chunk = conn.recv(256)
            except socket.timeout:
                continue
            if not chunk:
                break
            buf += chunk
            while b"\r" in buf:
                idx = buf.index(b"\r")
                raw = buf[:idx].decode("ascii", errors="replace")
                buf = buf[idx + 1:]
                if not raw.strip():
                    continue
                print(f"  <- {repr(raw)}")
                reply = process_command(raw, cs)
                if reply:
                    print(f"  -> {repr(reply)}")
                    conn.sendall(reply.encode("ascii"))
    except (ConnectionResetError, BrokenPipeError, OSError):
        pass
    finally:
        conn.close()
        print(f"[-] {addr[0]}:{addr[1]} disconnected")


def serve(port: int, stop: threading.Event):
    try:
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(("0.0.0.0", port))
        srv.listen(8)
        srv.settimeout(1.0)
        print(f"[*] Listening on 0.0.0.0:{port}")
        while not stop.is_set():
            try:
                conn, addr = srv.accept()
                threading.Thread(
                    target=handle_client, args=(conn, addr, port), daemon=True
                ).start()
            except socket.timeout:
                pass
    except PermissionError:
        print(f"[!] Port {port}: permission denied (ports <1024 need sudo)")
    except OSError as e:
        print(f"[!] Port {port}: {e}")
    finally:
        try:
            srv.close()
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Self-test (--test flag): connect locally and exercise the protocol
# ---------------------------------------------------------------------------

def self_test(port: int = 35000):
    print(f"\n--- Self-test against localhost:{port} ---")
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=3)
    except OSError as e:
        print(f"FAIL: cannot connect: {e}")
        return False

    def send(cmd):
        s.sendall((cmd + "\r").encode())
        time.sleep(0.1)
        data = b""
        s.settimeout(1.0)
        try:
            while True:
                chunk = s.recv(256)
                if not chunk:
                    break
                data += chunk
                if b">" in data:
                    break
        except socket.timeout:
            pass
        decoded = data.decode("ascii", errors="replace").strip()
        print(f"  {cmd!r:20s} -> {decoded!r}")
        return decoded

    ok = True

    r = send("ATI")
    if "ELM327" not in r:
        print("  FAIL: ATI did not return ELM327 version")
        ok = False

    r = send("ATE0")
    if "OK" not in r:
        print("  FAIL: ATE0 not OK")
        ok = False

    for pid_cmd, label in [
        ("010C", "RPM"),
        ("010D", "Speed"),
        ("0105", "Coolant"),
        ("0142", "Voltage"),
        ("0110", "MAF"),
        ("015E", "FuelRate"),
        ("03",   "DTC"),
    ]:
        r = send(pid_cmd)
        if "NO DATA" in r or r == "":
            print(f"  FAIL: {label} ({pid_cmd}) got NO DATA")
            ok = False

    s.close()
    print(f"\n{'PASS' if ok else 'FAIL'}: self-test {'passed' if ok else 'had failures'}")
    return ok


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

DEFAULT_PORTS = [35000, 3000, 23]

def main():
    ap = argparse.ArgumentParser(description="ELM327 WiFi adapter mock server")
    ap.add_argument("--port", type=int, action="append",
                    help="Port(s) to listen on (default: 35000 3000 23)")
    ap.add_argument("--test", action="store_true",
                    help="Run self-test against localhost:35000 then exit")
    args = ap.parse_args()

    ports = args.port if args.port else DEFAULT_PORTS

    stop = threading.Event()
    threads = []
    for p in ports:
        t = threading.Thread(target=serve, args=(p, stop), daemon=True)
        t.start()
        threads.append(t)

    if args.test:
        time.sleep(0.5)  # let servers start
        ok = self_test(ports[0])
        stop.set()
        sys.exit(0 if ok else 1)

    print("\nELM327 mock running. Ctrl-C to stop.\n")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping...")
        stop.set()


if __name__ == "__main__":
    main()
