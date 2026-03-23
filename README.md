# TeensyEprom

**Teensy 4.1 EPROM emulator — drop-in replacement for 27C256 / 27C512 chips.**

> **⚠ Work in Progress — Use at Your Own Risk**
>
> This project is under active development. Always keep a known-good
> programmed EPROM as backup before using this on a vehicle.
> If you find a bug, please [open an issue](https://github.com/dspl1236/TeensyEprom/issues).

## What It Does

Pull the EPROM chip out of your ECU, plug in a DIP-28 socket wired to a
Teensy 4.1, and serve ROM data from internal flash. Upload ROM files over
USB, switch between up to 16 maps with a button press — no chip burning,
no socket swapping, no SD card required.

Powers itself from the EPROM socket Vcc — no external supply needed.

## Tested ECUs

Any ECU using a DIP-28 27C256 or 27C512 with parallel address/data bus.

| ECU | Engine | EPROM | Status |
|-----|--------|-------|--------|
| 893906266D (MMS-05C) | 7A 20v | 27C256 | Primary target |
| 893906266B (MMS-04B) | 7A 20v early | 27C256 | Supported |
| 4A0906266 (MMS-100) | AAH 2.8 V6 | 27C256 | Supported |
| 8A0906266A (MMS-200) | AAH 2.8 V6 | 27C256 | Supported |
| 8A0906266B (MMS-300) | AAH 2.8 V6 | 27C512 | Supported |

Should also work with Bosch Motronic, Digifant, and other EPROM-based ECUs
with matching pinout.

## Hardware

| Part | Qty | Purpose |
|------|-----|---------|
| Teensy 4.1 | 1 | Microcontroller + internal flash storage |
| SN74HCT245N | 2 | Address bus level shift (5V → 3.3V) |
| 1kΩ resistor | 2 | /OE, /CE level clamp |
| 0.1µF ceramic | 3 | Bypass caps (3.3V rail) |
| DIP-28 socket | 1 | Mounts in ECU EPROM socket |
| Momentary button | 1 | Map switching (optional) |

**Total cost: ~$40**

SD card is optional — only needed as fallback storage.

## Quick Start

1. **Wire it up** — Follow [LITE_BUILD.md](LITE_BUILD.md) for connections
2. **Flash firmware** — `cd firmware && pio run --target upload`
3. **Upload ROMs** — `python app/teensy_eprom.py upload stock.bin`
4. **Install** — Plug DIP-28 socket into ECU where the EPROM was
5. **Go** — Key on, 3 slow LED blinks = running. Button press = next map.

## ROM Storage

**Primary: Internal Flash (LittleFS)**
ROMs stored on Teensy's 8MB program flash. Upload via USB from the desktop
app. No SD card, no moving parts — survives engine bay heat and vibration.

**Fallback: SD Card**
If internal flash is empty, checks SD card `/maps/` for .bin files.
Useful for initial bulk loading or field recovery.

## Power

```
DIP-28 pin 28 (Vcc, 5V from ECU) → Teensy Vin
DIP-28 pin 14 (GND)              → Teensy GND
```

## USB Serial Interface

Connect at 115200 baud, or use the desktop app:

```bash
pip install pyserial

python app/teensy_eprom.py                     # interactive mode
python app/teensy_eprom.py upload stock.bin     # upload ROM
python app/teensy_eprom.py upload stage2.bin    # upload another
python app/teensy_eprom.py list                 # list maps
python app/teensy_eprom.py switch 1             # switch to map 1
python app/teensy_eprom.py download 0 out.bin   # download map 0
```

Serial commands (direct):

```
INFO           — firmware version, storage status, active map
LIST           — show all maps (* = active)
MAP <n>        — switch to map index n
DUMP           — hex dump first 256 bytes of active ROM
UPLOAD name sz — upload ROM (raw bytes + CRC16)
DOWNLOAD <n>   — download map n
DELETE <n>     — delete map n
FORMAT         — delete all maps
SCAN           — rescan maps
```

## Project Structure

```
TeensyEprom/
├── README.md
├── LITE_BUILD.md          ← wiring guide + build instructions
├── firmware/
│   ├── platformio.ini     ← PlatformIO build config
│   └── src/
│       ├── config.h       ← pin assignments (single source of truth)
│       └── main.cpp       ← firmware (LittleFS + EPROM emulation)
├── app/
│   └── teensy_eprom.py    ← desktop ROM manager (Python + pyserial)
├── schematics/            ← interactive wiring diagrams
└── rom_files/             ← reference ROMs and documentation
```

## Related Projects

| Project | Description |
|---------|-------------|
| [HachiROM](https://github.com/dspl1236/HachiROM) | ROM editor — create the .bin files for this emulator |
| [M35Tool](https://github.com/dspl1236/M35Tool) | ROM editor for Bosch Motronic M3.x (VR6) |
| [DigiTool](https://github.com/dspl1236/DigiTool) | ROM editor for Bosch Digifant |
| [KWPBridge](https://github.com/dspl1236/KWPBridge) | KWP1281 diagnostic interface |
