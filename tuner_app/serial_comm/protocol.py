"""
serial_comm/protocol.py
Teensy USB Serial Protocol

PROTOCOL OVERVIEW
=================
All messages are newline-terminated ASCII.

TEENSY → PC (streaming, 10Hz):
  $DATA,rpm,map_kpa,tps_pct,iat_c,afr,fuel_trim_pct,knock_v,knock_retard_deg\n
  Example: $DATA,2450,87.3,23.5,28.1,14.72,+1.2,0.012,0.0\n

  $STATUS,ready|error,boot_ms,rom_file\n
  Example: $STATUS,ready,312,tune.bin\n

PC → TEENSY (commands):
  CMD:PING\n                          → TEENSY replies: ACK:PING\n
  CMD:GET_FUEL_MAP\n                  → TEENSY replies: MAP:FUEL,<288 comma-separated bytes>\n
  CMD:GET_TIMING_MAP\n                → TEENSY replies: MAP:TIMING,<288 comma-separated bytes>\n
  CMD:SET_CELL,fuel,row,col,value\n   → TEENSY replies: ACK:SET_CELL\n
  CMD:SET_CELL,timing,row,col,value\n → TEENSY replies: ACK:SET_CELL\n
  CMD:LOAD_ROM,filename\n             → TEENSY replies: ACK:LOAD_ROM or ERR:LOAD_ROM\n
  CMD:LIST_ROMS\n                     → TEENSY replies: ROMS:file1.bin,file2.bin,...\n
  CMD:SAVE_MAP\n                      → TEENSY replies: ACK:SAVE_MAP or ERR:SAVE_MAP\n
  CMD:CORRECTIONS_ON\n                → TEENSY replies: ACK:CORRECTIONS_ON\n
  CMD:CORRECTIONS_OFF\n               → TEENSY replies: ACK:CORRECTIONS_OFF\n
  CMD:SET_TARGET_AFR,value\n          → TEENSY replies: ACK:SET_TARGET_AFR\n
"""

import serial
import serial.tools.list_ports
import threading
import time
import queue
from dataclasses import dataclass, field
from typing import Optional, List, Callable

# Map dimensions — must match map_editor_tab.py
from ui.map_editor_tab import ROWS, COLS
MAP_SIZE = ROWS * COLS   # 288 bytes (18×16)


# ── Teensy USB IDs ─────────────────────────────────────────────────────────
TEENSY_VID = 0x16C0   # PJRC Teensy vendor ID
TEENSY_PIDS = [0x0483, 0x0487, 0x0478]  # Various Teensy serial modes


@dataclass
class LiveData:
    """Parsed live data frame from Teensy"""
    rpm:           int   = 0
    map_kpa:       float = 0.0
    tps_pct:       float = 0.0
    iat_c:         float = 0.0
    afr:           float = 0.0
    fuel_trim_pct: float = 0.0
    knock_v:       float = 0.0
    knock_retard:  float = 0.0
    timestamp:     float = field(default_factory=time.time)


@dataclass
class TeensyStatus:
    connected:  bool = False
    port:       str  = ""
    ready:      bool = False
    boot_ms:    int  = 0
    rom_file:   str  = ""
    error:      str  = ""


def list_ports() -> List[dict]:
    """Return all available serial ports with metadata."""
    ports = []
    for p in serial.tools.list_ports.comports():
        ports.append({
            "device":      p.device,
            "description": p.description,
            "vid":         p.vid,
            "pid":         p.pid,
            "is_teensy":   p.vid == TEENSY_VID if p.vid else False,
        })
    return ports


def find_teensy_port() -> Optional[str]:
    """Auto-detect Teensy 4.1 by USB VID/PID. Returns port string or None."""
    for p in serial.tools.list_ports.comports():
        if p.vid == TEENSY_VID:
            return p.device
    # Fallback: look for 'teensy' in description (case-insensitive)
    for p in serial.tools.list_ports.comports():
        if p.description and "teensy" in p.description.lower():
            return p.device
    return None


class TeensySerial:
    """
    Thread-safe serial connection to Teensy 4.1.
    Runs a background reader thread that parses incoming frames
    and calls registered callbacks.
    """

    BAUD = 115200

    def __init__(self):
        self._serial:   Optional[serial.Serial] = None
        self._thread:   Optional[threading.Thread] = None
        self._running:  bool = False
        self._lock:     threading.Lock = threading.Lock()
        self._cmd_queue: queue.Queue = queue.Queue()

        self.status = TeensyStatus()

        # Callbacks — set by UI
        self.on_live_data:   Optional[Callable[[LiveData], None]] = None
        self.on_status:      Optional[Callable[[TeensyStatus], None]] = None
        self.on_fuel_map:    Optional[Callable[[list], None]] = None
        self.on_timing_map:  Optional[Callable[[list], None]] = None
        self.on_rom_list:    Optional[Callable[[list], None]] = None
        self.on_log:         Optional[Callable[[str], None]] = None

        # Pending map response buffer
        self._map_buf:  list = []
        self._map_type: str  = ""

    # ── Connection ──────────────────────────────────────────────────────────

    def connect(self, port: str) -> bool:
        """Open serial port and start reader thread."""
        try:
            self._serial = serial.Serial(
                port=port,
                baudrate=self.BAUD,
                timeout=1.0,
                write_timeout=2.0,
            )
            self.status.connected = True
            self.status.port = port
            self.status.error = ""

            self._running = True
            self._thread = threading.Thread(
                target=self._reader_thread,
                daemon=True,
                name="TeensyReader"
            )
            self._thread.start()

            self._log(f"Connected to {port} at {self.BAUD} baud")

            # Ping to verify comms
            time.sleep(0.3)
            self.send_command("CMD:PING")
            return True

        except serial.SerialException as e:
            self.status.error = str(e)
            self._log(f"Connect failed: {e}")
            return False

    def disconnect(self):
        """Close connection and stop reader thread."""
        self._running = False
        if self._serial and self._serial.is_open:
            try:
                self._serial.close()
            except Exception:
                pass
        self.status.connected = False
        self.status.ready = False
        self._log("Disconnected")
        if self.on_status:
            self.on_status(self.status)

    def is_connected(self) -> bool:
        return self.status.connected and self._serial and self._serial.is_open

    # ── Commands ────────────────────────────────────────────────────────────

    def send_command(self, cmd: str):
        """Send a command to Teensy. Thread-safe."""
        if not self.is_connected():
            return
        try:
            with self._lock:
                self._serial.write((cmd + "\n").encode("ascii"))
            self._log(f"→ {cmd}")
        except serial.SerialException as e:
            self._log(f"Write error: {e}")
            self.disconnect()

    def request_fuel_map(self):
        self.send_command("CMD:GET_FUEL_MAP")

    def request_timing_map(self):
        self.send_command("CMD:GET_TIMING_MAP")

    def set_cell(self, map_type: str, row: int, col: int, value: int):
        """map_type: 'fuel' or 'timing'"""
        self.send_command(f"CMD:SET_CELL,{map_type},{row},{col},{value}")

    def load_rom(self, filename: str):
        self.send_command(f"CMD:LOAD_ROM,{filename}")

    def list_roms(self):
        self.send_command("CMD:LIST_ROMS")

    def save_map(self):
        self.send_command("CMD:SAVE_MAP")

    def corrections_on(self):
        self.send_command("CMD:CORRECTIONS_ON")

    def corrections_off(self):
        self.send_command("CMD:CORRECTIONS_OFF")

    def set_target_afr(self, afr: float):
        self.send_command(f"CMD:SET_TARGET_AFR,{afr:.2f}")

    # ── Reader thread ────────────────────────────────────────────────────────

    def _reader_thread(self):
        """Background thread: read lines from Teensy, parse and dispatch."""
        buf = b""
        while self._running:
            try:
                if not self._serial or not self._serial.is_open:
                    break
                chunk = self._serial.read(256)
                if not chunk:
                    continue
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    try:
                        self._parse_line(line.decode("ascii", errors="replace").strip())
                    except Exception as e:
                        self._log(f"Parse error: {e}")
            except serial.SerialException as e:
                self._log(f"Read error: {e}")
                break
            except Exception as e:
                self._log(f"Reader exception: {e}")
                break

        self.status.connected = False
        if self.on_status:
            self.on_status(self.status)

    def _parse_line(self, line: str):
        if not line:
            return

        self._log(f"← {line}")

        if line.startswith("$DATA,"):
            self._parse_data(line)

        elif line.startswith("$STATUS,"):
            self._parse_status(line)

        elif line.startswith("MAP:FUEL,"):
            vals = line[9:].split(",")
            try:
                data = [int(v) for v in vals if v.strip()]
                if len(data) == MAP_SIZE and self.on_fuel_map:
                    self.on_fuel_map(data)
                elif data:
                    self._log(f"Bad fuel map length: {len(data)} (expected {MAP_SIZE})")
            except ValueError:
                self._log("Bad fuel map data")

        elif line.startswith("MAP:TIMING,"):
            vals = line[11:].split(",")
            try:
                data = [int(v) for v in vals if v.strip()]
                if len(data) == MAP_SIZE and self.on_timing_map:
                    self.on_timing_map(data)
                elif data:
                    self._log(f"Bad timing map length: {len(data)} (expected {MAP_SIZE})")
            except ValueError:
                self._log("Bad timing map data")

        elif line.startswith("ROMS:"):
            roms = [r.strip() for r in line[5:].split(",") if r.strip()]
            if self.on_rom_list:
                self.on_rom_list(roms)

        elif line.startswith("ACK:"):
            self._log(f"ACK: {line[4:]}")

        elif line.startswith("ERR:"):
            self._log(f"ERR: {line[4:]}")

    def _parse_data(self, line: str):
        # $DATA,rpm,map_kpa,tps_pct,iat_c,afr,fuel_trim_pct,knock_v,knock_retard
        try:
            parts = line[6:].split(",")
            if len(parts) < 8:
                return
            d = LiveData(
                rpm           = int(parts[0]),
                map_kpa       = float(parts[1]),
                tps_pct       = float(parts[2]),
                iat_c         = float(parts[3]),
                afr           = float(parts[4]),
                fuel_trim_pct = float(parts[5]),
                knock_v       = float(parts[6]),
                knock_retard  = float(parts[7]),
            )
            if self.on_live_data:
                self.on_live_data(d)
        except (ValueError, IndexError) as e:
            self._log(f"Data parse error: {e} — {line}")

    def _parse_status(self, line: str):
        # $STATUS,ready|error,boot_ms,rom_file
        try:
            parts = line[8:].split(",")
            self.status.ready    = parts[0].strip() == "ready"
            self.status.boot_ms  = int(parts[1]) if len(parts) > 1 else 0
            self.status.rom_file = parts[2].strip() if len(parts) > 2 else ""
            if self.on_status:
                self.on_status(self.status)
        except Exception as e:
            self._log(f"Status parse error: {e}")

    def _log(self, msg: str):
        if self.on_log:
            self.on_log(msg)
