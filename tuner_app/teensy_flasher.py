"""
teensy_flasher.py
─────────────────
Flash a .hex file to a Teensy 4.1 via the bundled teensy_loader_cli.

Usage:
    flasher = TeensyFlasher()
    flasher.flash("FULL", on_progress=cb, on_done=done_cb)
    flasher.flash("LITE", on_progress=cb, on_done=done_cb)
"""

import os
import sys
import subprocess
import threading
from typing import Callable, Optional

# ── Locate bundled resources ──────────────────────────────────────────────────

def _resource_path(relative: str) -> str:
    """
    Works both from source tree and from a PyInstaller --onefile exe.
    PyInstaller unpacks to sys._MEIPASS at runtime.
    """
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, relative)


# Paths inside the bundle
_CLI_PATH  = _resource_path(os.path.join("tools", "teensy_loader_cli.exe"))
_HEX_FULL  = _resource_path(os.path.join("firmware", "teensy_full.hex"))
_HEX_LITE  = _resource_path(os.path.join("firmware", "teensy_lite.hex"))

MCU = "TEENSY41"


class FlashError(Exception):
    pass


class TeensyFlasher:
    """
    Non-blocking Teensy hex flasher.
    Runs teensy_loader_cli in a background thread and reports progress
    via callbacks so the Qt UI stays responsive.
    """

    def __init__(self):
        self._thread: Optional[threading.Thread] = None
        self._cancel = False

    # ── Public API ────────────────────────────────────────────────────────────

    def is_available(self) -> bool:
        """True if teensy_loader_cli.exe is bundled."""
        return os.path.isfile(_CLI_PATH)

    def hex_exists(self, mode: str) -> bool:
        """True if the .hex for FULL or LITE exists in the bundle."""
        return os.path.isfile(self._hex_for(mode))

    def flash(
        self,
        mode: str,                               # "FULL" or "LITE"
        on_progress: Optional[Callable] = None,  # fn(str) — log line
        on_done: Optional[Callable]     = None,  # fn(success: bool, msg: str)
    ):
        """
        Flash the selected firmware in a background thread.
        The Teensy must be in bootloader mode (press reset, or app sends reboot cmd).
        """
        if self._thread and self._thread.is_alive():
            if on_done:
                on_done(False, "Flash already in progress.")
            return

        self._cancel = False
        self._thread = threading.Thread(
            target=self._flash_worker,
            args=(mode, on_progress, on_done),
            daemon=True,
        )
        self._thread.start()

    def cancel(self):
        self._cancel = True

    # ── Internal ──────────────────────────────────────────────────────────────

    @staticmethod
    def _hex_for(mode: str) -> str:
        return _HEX_FULL if mode.upper() == "FULL" else _HEX_LITE

    def _flash_worker(self, mode, on_progress, on_done):
        def log(msg):
            if on_progress:
                on_progress(msg)

        hex_path = self._hex_for(mode)

        # Preflight checks
        if not self.is_available():
            if on_done:
                on_done(False, f"teensy_loader_cli.exe not found at:\n{_CLI_PATH}")
            return

        if not os.path.isfile(hex_path):
            if on_done:
                on_done(False, f"Firmware hex not found:\n{hex_path}")
            return

        log(f"[FLASH] Starting — mode={mode}")
        log(f"[FLASH] Hex: {os.path.basename(hex_path)}")
        log(f"[FLASH] Loader: {os.path.basename(_CLI_PATH)}")
        log("[FLASH] Waiting for Teensy bootloader…")

        cmd = [
            _CLI_PATH,
            f"--mcu={MCU}",
            "-w",          # wait for device
            "-v",          # verbose
            hex_path,
        ]

        try:
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
            )

            for line in proc.stdout:
                line = line.rstrip()
                if line:
                    log(f"[FLASH] {line}")
                if self._cancel:
                    proc.terminate()
                    if on_done:
                        on_done(False, "Flash cancelled.")
                    return

            proc.wait()

            if proc.returncode == 0:
                log("[FLASH] ✓ Flash complete — Teensy rebooting into new firmware")
                if on_done:
                    on_done(True, f"Teensy flashed with {mode} firmware successfully.")
            else:
                msg = f"teensy_loader_cli exited with code {proc.returncode}"
                log(f"[FLASH] ✗ {msg}")
                if on_done:
                    on_done(False, msg)

        except FileNotFoundError:
            msg = f"Could not launch: {_CLI_PATH}"
            log(f"[FLASH] ✗ {msg}")
            if on_done:
                on_done(False, msg)
        except Exception as e:
            log(f"[FLASH] ✗ Exception: {e}")
            if on_done:
                on_done(False, str(e))


# ── Firmware mode detection ───────────────────────────────────────────────────

class FirmwareMode:
    """
    Detected mode returned by ident_from_line().
    """
    UNKNOWN  = "UNKNOWN"
    FULL     = "FULL"       # IDENT:TEENSY_FULL:vX.Y.Z
    LITE     = "LITE"       # IDENT:TEENSY_LITE:vX.Y.Z

    def __init__(self, mode: str = UNKNOWN, version: str = ""):
        self.mode    = mode
        self.version = version

    @staticmethod
    def from_ident(line: str) -> "FirmwareMode":
        """
        Parse the ident string sent by firmware on USB connect.
        Returns FirmwareMode(UNKNOWN) if line doesn't match.
        """
        line = line.strip()
        if line.startswith("IDENT:TEENSY_FULL:"):
            return FirmwareMode(FirmwareMode.FULL, line.split(":", 2)[2])
        if line.startswith("IDENT:TEENSY_LITE:"):
            return FirmwareMode(FirmwareMode.LITE, line.split(":", 2)[2])
        return FirmwareMode()

    @property
    def is_full(self):  return self.mode == self.FULL
    @property
    def is_lite(self):  return self.mode == self.LITE
    @property
    def is_known(self): return self.mode != self.UNKNOWN

    def __repr__(self):
        return f"FirmwareMode({self.mode}, {self.version})"
