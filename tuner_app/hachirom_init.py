"""
hachirom_init.py
================
Ensures the HachiROM submodule is on sys.path so tuner_app can import it.
Import this at the top of tuner_app/main.py (already done in updated main.py).

HachiROM provides:
  - ROM detection (CRC32, reset vector, heuristic)
  - Map read/write with correct formulas for 266D, 266B, AAH
  - Checksum verification and correction
  - ROM compare / diff
  - .034 file unscramble
  - Bridge functions matching Teensy serial protocol

Usage:
    from hachirom.bridge import (
        get_flat_fuel_map,
        get_flat_timing_map,
        set_cell,
        apply_checksum,
        verify_checksum,
        compare_roms,
        unscramble_034,
        detect,
        load_bin,
        save_bin,
    )
"""

import sys
import os
from pathlib import Path

def init():
    """Add hachirom submodule to sys.path. Call once at startup."""
    # tuner_app/hachirom_init.py → project root → hachirom/
    root = Path(__file__).parent.parent
    hachirom_path = root / "hachirom"
    if hachirom_path.exists() and str(root) not in sys.path:
        sys.path.insert(0, str(root))

init()
