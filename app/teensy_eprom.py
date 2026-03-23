#!/usr/bin/env python3
"""
TeensyEprom — Desktop ROM Manager

Upload, list, and switch ROM files on a TeensyEprom device.
ROMs are stored on the Teensy's internal flash (LittleFS).

Usage:
    python teensy_eprom.py                     # interactive mode
    python teensy_eprom.py list                # list maps
    python teensy_eprom.py switch 2            # switch to map index 2
    python teensy_eprom.py upload tune.bin     # upload ROM file
    python teensy_eprom.py info                # device info
    python teensy_eprom.py monitor             # live serial monitor

Requires: pip install pyserial
"""

import sys
import os
import time
import zlib
import struct
import argparse

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("pyserial required: pip install pyserial")
    sys.exit(1)


BAUD = 115200
TIMEOUT = 2.0
IDENT = "TeensyEprom"
ROM_MIN = 32768
ROM_MAX = 65536


# ---------------------------------------------------------------------------
# Device detection
# ---------------------------------------------------------------------------

def find_teensy(port=None):
    """Auto-detect TeensyEprom on USB serial."""
    if port:
        try:
            ser = serial.Serial(port, BAUD, timeout=TIMEOUT)
            time.sleep(0.5)
            return ser, port
        except serial.SerialException as e:
            print(f"Error: {e}")
            sys.exit(1)

    ports = serial.tools.list_ports.comports()
    candidates = []
    for p in ports:
        desc = (p.description or "").lower()
        vid = p.vid or 0
        # Teensy 4.1 USB VID:PID = 16C0:0483
        if vid == 0x16C0 or "teensy" in desc or "serial" in desc:
            candidates.append(p.device)

    for dev in candidates:
        try:
            ser = serial.Serial(dev, BAUD, timeout=TIMEOUT)
            time.sleep(0.5)
            ser.reset_input_buffer()
            ser.write(b"INFO\n")
            time.sleep(0.3)
            resp = ser.read(ser.in_waiting or 256).decode("utf-8", errors="replace")
            if IDENT in resp:
                return ser, dev
            ser.close()
        except (serial.SerialException, OSError):
            continue

    return None, None


def send_cmd(ser, cmd, wait=0.3):
    """Send text command, return response."""
    ser.reset_input_buffer()
    ser.write(f"{cmd}\n".encode())
    time.sleep(wait)
    resp = ser.read(ser.in_waiting or 4096).decode("utf-8", errors="replace")
    return resp.strip()


# ---------------------------------------------------------------------------
# Upload — binary transfer with CRC32
# ---------------------------------------------------------------------------

def upload_rom(ser, local_path, remote_name=None):
    """Upload a .bin ROM file to the Teensy's internal flash.

    Protocol:
        Host: UPLOAD /maps/<name>.bin <size>\n
        Dev:  READY <size>\n
        Host: <size raw bytes><4-byte CRC32 big-endian>
        Dev:  OK ... CRC32=...\n
    """
    if not os.path.exists(local_path):
        print(f"Error: {local_path} not found")
        return False

    data = open(local_path, "rb").read()
    size = len(data)

    if size < ROM_MIN or size > ROM_MAX:
        print(f"Error: file must be {ROM_MIN}-{ROM_MAX} bytes (got {size})")
        return False

    # Determine remote filename
    if remote_name is None:
        remote_name = os.path.basename(local_path)
    if not remote_name.startswith("/maps/"):
        remote_name = f"/maps/{remote_name}"

    print(f"Uploading {local_path} ({size}B) -> {remote_name}")

    # Compute CRC32
    crc = zlib.crc32(data) & 0xFFFFFFFF
    crc_bytes = struct.pack(">I", crc)
    print(f"  CRC32: {crc:08X}")

    # Send UPLOAD command
    ser.reset_input_buffer()
    cmd = f"UPLOAD {remote_name} {size}\n"
    ser.write(cmd.encode())

    # Wait for READY
    deadline = time.time() + 5.0
    buf = b""
    while time.time() < deadline:
        if ser.in_waiting:
            buf += ser.read(ser.in_waiting)
            if b"\n" in buf:
                break
        time.sleep(0.01)

    resp = buf.decode("utf-8", errors="replace").strip()
    if not resp.startswith("READY"):
        print(f"  Error: expected READY, got: {resp}")
        return False

    print(f"  Device ready, sending {size} bytes...")

    # Send raw data + CRC32
    # Send in chunks to avoid serial buffer overflow
    chunk_size = 4096
    sent = 0
    while sent < size:
        end = min(sent + chunk_size, size)
        ser.write(data[sent:end])
        sent = end
        pct = sent * 100 // size
        print(f"\r  Sent: {sent}/{size} ({pct}%)", end="", flush=True)
        time.sleep(0.01)  # let serial drain

    # Send CRC32 (4 bytes, big-endian)
    ser.write(crc_bytes)
    print(f"\r  Sent: {size}/{size} (100%) + CRC32")

    # Wait for OK/ERR
    deadline = time.time() + 10.0
    buf = b""
    while time.time() < deadline:
        if ser.in_waiting:
            buf += ser.read(ser.in_waiting)
            if b"\n" in buf:
                break
        time.sleep(0.01)

    # Read any remaining output (map rescan, etc)
    time.sleep(0.3)
    if ser.in_waiting:
        buf += ser.read(ser.in_waiting)

    resp = buf.decode("utf-8", errors="replace").strip()
    lines = resp.split("\n")
    for line in lines:
        print(f"  {line.strip()}")

    ok = any(line.strip().startswith("OK") for line in lines)
    if ok:
        print("  Upload complete!")
    else:
        print("  Upload FAILED")
    return ok


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_info(ser):
    print(send_cmd(ser, "INFO"))

def cmd_list(ser):
    print(send_cmd(ser, "LIST"))

def cmd_switch(ser, idx):
    print(send_cmd(ser, f"MAP {idx}"))

def cmd_dump(ser):
    print(send_cmd(ser, "DUMP", wait=0.5))

def cmd_delete(ser, name):
    if not name.startswith("/maps/"):
        name = f"/maps/{name}"
    resp = send_cmd(ser, f"DELETE {name}")
    print(resp)

def cmd_format(ser):
    confirm = input("This will ERASE all ROMs from flash. Type 'yes' to confirm: ")
    if confirm.strip().lower() == "yes":
        print(send_cmd(ser, "FORMAT", wait=2.0))
    else:
        print("Cancelled.")

def cmd_monitor(ser):
    """Live serial monitor — press Ctrl+C to exit."""
    print("--- TeensyEprom monitor (Ctrl+C to exit) ---")
    try:
        while True:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
                print(data, end="", flush=True)
            time.sleep(0.05)
    except KeyboardInterrupt:
        print("\n--- closed ---")


# ---------------------------------------------------------------------------
# Interactive REPL
# ---------------------------------------------------------------------------

def interactive(ser, port):
    print(f"Connected to {port}")
    print("Commands: list, switch <n>, upload <file>, delete <file>, info, dump, format, monitor, quit")
    print()
    cmd_info(ser)
    print()

    while True:
        try:
            line = input("eprom> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if not line:
            continue

        low = line.lower()
        if low in ("q", "quit", "exit"):
            break
        elif low in ("l", "list"):
            cmd_list(ser)
        elif low.startswith("s ") or low.startswith("switch "):
            parts = line.split()
            if len(parts) == 2 and parts[1].isdigit():
                cmd_switch(ser, int(parts[1]))
            else:
                print("Usage: switch <map_index>")
        elif low.startswith("u ") or low.startswith("upload "):
            parts = line.split(None, 2)
            if len(parts) >= 2:
                local = parts[1]
                remote = parts[2] if len(parts) == 3 else None
                upload_rom(ser, local, remote)
            else:
                print("Usage: upload <local_file> [remote_name]")
        elif low.startswith("delete ") or low.startswith("del "):
            parts = line.split(None, 1)
            if len(parts) == 2:
                cmd_delete(ser, parts[1])
            else:
                print("Usage: delete <filename>")
        elif low == "format":
            cmd_format(ser)
        elif low in ("i", "info"):
            cmd_info(ser)
        elif low in ("d", "dump"):
            cmd_dump(ser)
        elif low in ("m", "monitor"):
            cmd_monitor(ser)
        elif low == "help":
            print("  list / l             — show maps on device")
            print("  switch <n> / s <n>   — switch to map index n")
            print("  upload <file>        — upload .bin ROM to device")
            print("  delete <file>        — remove a map from flash")
            print("  format               — erase all ROMs from flash")
            print("  info / i             — firmware version + status")
            print("  dump / d             — hex dump first 256 bytes")
            print("  monitor / m          — live serial output")
            print("  quit / q             — exit")
        else:
            # Pass raw command
            print(send_cmd(ser, line.upper()))


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="TeensyEprom — Desktop ROM Manager",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Examples:\n"
               "  %(prog)s upload tune.bin           Upload ROM to device\n"
               "  %(prog)s upload tune.bin my_map.bin Upload with custom name\n"
               "  %(prog)s list                      List maps on device\n"
               "  %(prog)s switch 2                  Switch to map index 2\n"
    )
    parser.add_argument("command", nargs="?", default="interactive",
                        choices=["list", "switch", "upload", "info", "dump",
                                 "delete", "format", "monitor", "interactive"],
                        help="Command (default: interactive)")
    parser.add_argument("args", nargs="*", help="Command arguments")
    parser.add_argument("-p", "--port", help="Serial port (auto-detect if omitted)")

    args = parser.parse_args()

    print(f"Searching for {IDENT}...")
    ser, port = find_teensy(args.port)
    if ser is None:
        print("TeensyEprom not found. Check USB connection.")
        print("Available ports:")
        for p in serial.tools.list_ports.comports():
            print(f"  {p.device}: {p.description}")
        sys.exit(1)

    print(f"Found on {port}\n")

    try:
        if args.command == "list":
            cmd_list(ser)
        elif args.command == "switch":
            if args.args and args.args[0].isdigit():
                cmd_switch(ser, int(args.args[0]))
            else:
                print("Usage: teensy_eprom.py switch <map_index>")
        elif args.command == "upload":
            if args.args:
                local = args.args[0]
                remote = args.args[1] if len(args.args) > 1 else None
                ok = upload_rom(ser, local, remote)
                sys.exit(0 if ok else 1)
            else:
                print("Usage: teensy_eprom.py upload <file.bin> [remote_name]")
        elif args.command == "delete":
            if args.args:
                cmd_delete(ser, args.args[0])
            else:
                print("Usage: teensy_eprom.py delete <filename>")
        elif args.command == "format":
            cmd_format(ser)
        elif args.command == "info":
            cmd_info(ser)
        elif args.command == "dump":
            cmd_dump(ser)
        elif args.command == "monitor":
            cmd_monitor(ser)
        else:
            interactive(ser, port)
    finally:
        ser.close()


if __name__ == "__main__":
    main()
