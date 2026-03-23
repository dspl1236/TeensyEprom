# TeensyEprom

**Teensy 4.1 EPROM emulator — drop-in replacement for 27C256 / 27C512 chips.**

> **⚠ Work in Progress — Use at Your Own Risk**
>
> This project is under active development. Always keep a known-good
> programmed EPROM as backup before using this on a vehicle.
> If you find a bug, please [open an issue](https://github.com/dspl1236/TeensyEprom/issues).

## What It Does

Pull the EPROM chip out of your ECU, plug in a DIP-28 socket wired to a
Teensy 4.1, and serve ROM data from the Teensy's internal flash. Switch
between up to 16 map files with a button press or USB command — no chip
burning, no socket swapping.

Powers itself from the EPROM socket Vcc — no external supply needed.

## Storage

**Primary: Internal flash (LittleFS)** — ROMs are stored on the Teensy's
8MB program flash. No SD card needed for normal operation. Upload ROMs from
your PC using the desktop app or USB serial. Survives heat, vibration, and
moisture — no moving parts, no card to unseat.

**Fallback: SD card** — if internal flash has no maps at boot, the Teensy
checks the SD card. Useful for initial setup or field recovery.

## Tested ECUs

Any ECU using a DIP-28 27C256 or 27C512 with parallel address/data bus.

| ECU | Engine | EPROM | Status |
|-----|--------|-------|--------|
| 893906266D (MMS-05C) | 7A 20v | 27C256 | Primary target |
| 893906266B (MMS-04B) | 7A 20v early | 27C256 | Supported |
| 4A0906266 (MMS-100) | AAH 2.8 V6 | 27C256 | Supported |
| 8A0906266A (MMS-200) | AAH 2.8 V6 | 27C256 | Supported |
| 8A0906266B (MMS-300) | AAH 2.8 V6 | 27C512 | Supported |

## Hardware

| Part | Qty | Purpose |
|------|-----|---------|
| Teensy 4.1 | 1 | Microcontroller + flash storage |
| SN74HCT245N | 2 | Address bus level shift (5V → 3.3V) |
| 1kΩ resistor | 2 | /OE, /CE level clamp |
| 0.1µF ceramic | 3 | Bypass caps (3.3V rail) |
| DIP-28 socket | 1 | Mounts in ECU EPROM socket |
| Momentary button | 1 | Map switching (optional) |

**Total cost: ~$40** — SD card not required.

## Quick Start

1. **Wire it up** — Follow [LITE_BUILD.md](LITE_BUILD.md) for connections
2. **Flash firmware** — `cd firmware && pio run --target upload`
3. **Upload a ROM** — `python app/teensy_eprom.py upload your_tune.bin`
4. **Install** — Plug DIP-28 socket into ECU where the EPROM was
5. **Go** — Key on, 3 slow LED blinks = running. Button press = next map.

## Power

The Teensy powers itself from the EPROM socket:

```
DIP-28 pin 28 (Vcc, 5V from ECU) → Teensy Vin
DIP-28 pin 14 (GND)              → Teensy GND
```

Teensy's onboard regulator drops 5V to 3.3V for the MCU and 74HCT245s.
USB can be connected simultaneously for serial commands.

## Desktop App

```bash
pip install pyserial

python app/teensy_eprom.py                          # interactive mode
python app/teensy_eprom.py upload tune.bin           # upload ROM
python app/teensy_eprom.py upload tune.bin stage2.bin  # upload with custom name
python app/teensy_eprom.py list                      # list maps
python app/teensy_eprom.py switch 2                  # switch to map index 2
python app/teensy_eprom.py info                      # device info
python app/teensy_eprom.py delete /maps/old.bin      # remove a map
```

Upload verifies data integrity with CRC32 — no silent corruption.

## USB Serial Commands

Connect at 115200 baud:

```
INFO                          — firmware version + storage status
LIST                          — show all maps (* = active)
MAP 2                         — switch to map index 2
DUMP                          — hex dump first 256 bytes
UPLOAD /maps/tune.bin 32768   — binary upload (use desktop app)
DELETE /maps/old.bin          — remove a map
FORMAT                        — erase all ROMs from flash
RESCAN                        — re-scan map directory
```

## LED Status

| Pattern | Meaning |
|---------|---------|
| 3 slow blinks at boot | Running, ROM loaded |
| N blinks after switch | Switched to map index N |
| Solid on | Waiting for first ROM upload |
| Fast blink (panic) | Fatal error — no storage |

## Project Structure

```
TeensyEprom/
├── README.md
├── LITE_BUILD.md              ← wiring guide + build instructions
├── firmware/
│   ├── platformio.ini         ← PlatformIO build config
│   └── src/
│       ├── config.h           ← pin assignments + storage config
│       └── main.cpp           ← firmware (LittleFS + ISR + serial)
├── app/
│   └── teensy_eprom.py        ← desktop ROM manager (upload/switch)
├── schematics/                ← interactive wiring diagrams
└── rom_files/                 ← reference ROMs and documentation
```

## Related Projects

| Project | Description |
|---------|-------------|
| [HachiROM](https://github.com/dspl1236/HachiROM) | ROM editor — create the .bin files for this emulator |
| [M35Tool](https://github.com/dspl1236/M35Tool) | ROM editor for Bosch Motronic M3.x (VR6) |
| [DigiTool](https://github.com/dspl1236/DigiTool) | ROM editor for Bosch Digifant |
| [KWPBridge](https://github.com/dspl1236/KWPBridge) | KWP1281 diagnostic interface |
