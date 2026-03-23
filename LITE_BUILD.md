# TeensyEprom Lite — Build Guide

> **⚠ Work in Progress — Use at Your Own Risk**
>
> This is a DIY EPROM emulator. Verify every connection before powering on.
> Always keep a known-good programmed EPROM as backup.

## What It Does

Replaces the 27C256 / 27C512 EPROM in Hitachi MMS ECUs with a Teensy 4.1
that serves ROM data from SD card. Supports multiple map files and switching
between them with a button press.

**Compatible ECUs:** 893906266D (7A 20v), 893906266B (7A early),
4A0906266 (AAH V6), 8A0906266A (MMS-200), 8A0906266B (MMS-300).

## BOM — Teensy Lite Only

| Ref | Part | Qty | Notes |
|-----|------|-----|-------|
| U0 | Teensy 4.1 (with pins) | 1 | Must have SD card slot |
| U1 | SN74HCT245N (DIP-20) | 1 | Address low byte |
| U2 | SN74HCT245N (DIP-20) | 1 | Address high byte |
| J1 | 28-pin DIP socket | 1 | Goes in ECU (replaces EPROM) |
| R1 | 1kΩ resistor | 1 | /OE level clamp |
| R2 | 1kΩ resistor | 1 | /CE level clamp |
| C1 | 0.1µF ceramic | 1 | Bypass cap — U1 Vcc |
| C2 | 0.1µF ceramic | 1 | Bypass cap — U2 Vcc |
| C3 | 0.1µF ceramic | 1 | Bypass cap — Teensy 3.3V |
| SW1 | Momentary pushbutton | 1 | Map switch (optional) |
| — | Breadboard + jumper wires | — | |
| — | Micro SD card (FAT32) | 1 | ROM files |
| — | Ribbon cable or hookup wire | ~30 | DIP socket → breadboard |

**Total cost:** ~$35 (Teensy) + ~$5 (parts) = **~$40**

## Circuit Design

```
DIP-28 Socket (in ECU)          74HCT245 × 2              Teensy 4.1
 ┌────────────────┐            (3.3V Vcc)              ┌──────────────┐
 │ A0-A7   (5V) ──┼── U1 A-side ──→ B-side ───────────┼── pins 2-9   │
 │ A8-A15  (5V) ──┼── U2 A-side ──→ B-side ───────────┼── pins 10-12,│
 │                 │                                    │    24-28     │
 │ D0-D7        ──┼── direct (no buffer) ──────────────┼── pins 14-21 │
 │                 │                                    │              │
 │ /OE    (5V) ───┼── 1kΩ resistor ────────────────────┼── pin 29     │
 │ /CE    (5V) ───┼── 1kΩ resistor ────────────────────┼── pin 30     │
 │                 │                                    │              │
 │ GND ───────────┼── common ground ───────────────────┼── GND        │
 │ Vcc ───────────┼── 5V (ECU supplies) ──→ not used   │              │
 └────────────────┘    (Teensy uses USB or own supply)  └──────────────┘
```

### Level Shifting

The Teensy 4.1 is **3.3V only — NOT 5V tolerant**. The ECU bus is 5V TTL.

| Signal | Direction | Method |
|--------|-----------|--------|
| Address A0-A15 | ECU → Teensy | 74HCT245 @ 3.3V Vcc (clamps 5V to ~3.8V) |
| Data D0-D7 | Teensy → ECU | Direct — 3.3V output > 2.0V TTL VIH threshold |
| /OE, /CE | ECU → Teensy | 1kΩ series resistor (1.7mA clamp, within spec) |

### Why No Buffer on Data Bus?

The ECU's HD6303 CPU has TTL-level inputs with VIH = 2.0V. Teensy outputs 3.3V,
which exceeds this threshold. The data bus works without a level shifter.

If you want full 5V output (belt-and-suspenders), add a third 74HCT245 at **5V Vcc**
with DIR=HIGH (B→A) for the data bus. This is optional.

## Wiring Table

### U1 — 74HCT245 #1 (Address Low Byte)

| U1 Pin | U1 Function | Connects To |
|--------|-------------|-------------|
| 1 | DIR | GND (A→B direction) |
| 2 | A1 (input) | DIP-28 pin 10 (A0) |
| 3 | A2 (input) | DIP-28 pin 9 (A1) |
| 4 | A3 (input) | DIP-28 pin 8 (A2) |
| 5 | A4 (input) | DIP-28 pin 7 (A3) |
| 6 | A5 (input) | DIP-28 pin 6 (A4) |
| 7 | A6 (input) | DIP-28 pin 5 (A5) |
| 8 | A7 (input) | DIP-28 pin 4 (A6) |
| 9 | A8 (input) | DIP-28 pin 3 (A7) |
| 10 | GND | GND |
| 11 | B8 (output) | Teensy pin 9 (A7) |
| 12 | B7 (output) | Teensy pin 8 (A6) |
| 13 | B6 (output) | Teensy pin 7 (A5) |
| 14 | B5 (output) | Teensy pin 6 (A4) |
| 15 | B4 (output) | Teensy pin 5 (A3) |
| 16 | B3 (output) | Teensy pin 4 (A2) |
| 17 | B2 (output) | Teensy pin 3 (A1) |
| 18 | B1 (output) | Teensy pin 2 (A0) |
| 19 | /OE | GND (always enabled) |
| 20 | Vcc | **3.3V** (from Teensy 3.3V pin) |

### U2 — 74HCT245 #2 (Address High Byte)

| U2 Pin | U2 Function | Connects To |
|--------|-------------|-------------|
| 1 | DIR | GND (A→B direction) |
| 2 | A1 (input) | DIP-28 pin 25 (A8) |
| 3 | A2 (input) | DIP-28 pin 24 (A9) |
| 4 | A3 (input) | DIP-28 pin 21 (A10) |
| 5 | A4 (input) | DIP-28 pin 23 (A11) |
| 6 | A5 (input) | DIP-28 pin 2 (A12) |
| 7 | A6 (input) | DIP-28 pin 26 (A13) |
| 8 | A7 (input) | DIP-28 pin 27 (A14) |
| 9 | A8 (input) | DIP-28 pin 1 (A15) |
| 10 | GND | GND |
| 11 | B8 (output) | Teensy pin 28 (A15) |
| 12 | B7 (output) | Teensy pin 27 (A14) |
| 13 | B6 (output) | Teensy pin 26 (A13) |
| 14 | B5 (output) | Teensy pin 25 (A12) |
| 15 | B4 (output) | Teensy pin 24 (A11) |
| 16 | B3 (output) | Teensy pin 12 (A10) |
| 17 | B2 (output) | Teensy pin 11 (A9) |
| 18 | B1 (output) | Teensy pin 10 (A8) |
| 19 | /OE | GND (always enabled) |
| 20 | Vcc | **3.3V** (from Teensy 3.3V pin) |

### Data Bus — Direct (No Buffer)

| DIP-28 Pin | Signal | Teensy Pin |
|------------|--------|------------|
| 11 | D0 | 14 |
| 12 | D1 | 15 |
| 13 | D2 | 16 |
| 15 | D3 | 17 |
| 16 | D4 | 18 |
| 17 | D5 | 19 |
| 18 | D6 | 20 |
| 19 | D7 | 21 |

### Control Signals

| DIP-28 Pin | Signal | Teensy Pin | Notes |
|------------|--------|------------|-------|
| 22 | /OE | 29 | Via 1kΩ series resistor |
| 20 | /CE | 30 | Via 1kΩ series resistor |

### Power and Ground

| DIP-28 Pin | Signal | Connects To |
|------------|--------|-------------|
| 14 | GND | Common ground (Teensy GND) |
| 28 | Vcc | **Teensy Vin** (5V from ECU powers everything) |

> **DIP-28 Pin 28 (Vcc):** The ECU supplies 5V on this pin. Connect directly
> to Teensy Vin — the onboard regulator drops to 3.3V for the MCU and
> 74HCT245s. No USB power needed once installed. USB can be connected
> simultaneously for ROM management (Teensy handles dual power safely).

### Map Switch Button (Optional)

| Component | From | To |
|-----------|------|----|
| SW1 terminal 1 | Teensy pin 31 | — |
| SW1 terminal 2 | GND | — |

Internal pull-up enabled in firmware. Short press = next map. Long press (>1s) = previous map.

## Bypass Capacitors

Place 0.1µF ceramic caps as close as possible to each IC:

| Cap | IC | Vcc Pin | GND Pin |
|-----|----|---------|---------|
| C1 | U1 (74HCT245) | Pin 20 | Pin 10 |
| C2 | U2 (74HCT245) | Pin 20 | Pin 10 |
| C3 | Teensy | 3.3V | GND |

## ROM Storage

ROMs are stored in the Teensy's internal program flash (LittleFS). No SD card
required for daily use — flash has no moving parts, no contacts to oxidize,
and survives engine bay heat and vibration.

**16 slots** available (0–15). Each holds a 32KB or 64KB ROM file.
32KB files are automatically mirrored to fill 64KB (A15-agnostic).

### Loading ROMs — Desktop App (recommended)

```bash
pip install pyserial                             # one-time setup
python app/teensy_eprom.py upload 0 stock.bin    # upload to slot 0
python app/teensy_eprom.py upload 1 stage1.bin   # upload to slot 1
python app/teensy_eprom.py upload 2 stage2.bin   # upload to slot 2
python app/teensy_eprom.py list                  # verify
python app/teensy_eprom.py switch 1              # activate slot 1
```

Or run `python app/teensy_eprom.py` for interactive mode.

### Loading ROMs — SD Card Fallback

If the flash is empty at boot and an SD card is inserted, the firmware will
automatically import `.bin` files from the `maps/` directory into flash slots.
This is useful for first-time setup or recovery.

```
SD (FAT32):
└── maps/
    ├── stock.bin       → imported to slot 0
    ├── stage1.bin      → imported to slot 1
    └── stage2.bin      → imported to slot 2
```

After import, the SD card can be removed. ROMs live in flash permanently
until overwritten or deleted via USB.

## Firmware Upload

```bash
cd firmware
pio run --target upload
```

Or use Arduino IDE with Teensy 4.1 board selected.

## USB Serial Commands

Connect via serial monitor at 115200 baud, or use the desktop app:

| Command | Description |
|---------|-------------|
| `INFO` | Firmware version, active slot, status |
| `LIST` | List all ROM slots (* = active) |
| `MAP 2` | Switch to slot 2 |
| `UPLOAD 0 32768` | Upload 32KB ROM to slot 0 (binary follows) |
| `DELETE 3` | Erase slot 3 |
| `DUMP 0 256` | Hex dump 256 bytes from offset 0 |
| `IMPORT` | Import ROMs from SD card |
| `FORMAT` | Erase all slots |

## LED Status

| Pattern | Meaning |
|---------|---------|
| 3 slow blinks | Boot OK, emulation active |
| N blinks | Switched to slot N |
| 10 fast blinks | No ROMs loaded — upload via USB or insert SD |
| Rapid blink (continuous) | Fatal error (flash init failed) |

## First Power-On Checklist

1. [ ] Verify all wiring against tables above (twice)
2. [ ] DIP-28 socket NOT installed in ECU yet
3. [ ] Connect Teensy via USB
4. [ ] Flash firmware: `cd firmware && pio run --target upload`
5. [ ] Open serial monitor — confirm "TeensyEprom v2.0"
6. [ ] Upload a ROM: `python app/teensy_eprom.py upload 0 your_stock.bin`
7. [ ] Verify: type `DUMP` — should show non-zero data
8. [ ] Measure voltages: 3.3V on U1/U2 Vcc, GND on DIR and /OE pins
9. [ ] Install DIP-28 socket in ECU, connect to Teensy
10. [ ] Connect DIP-28 pin 28 (Vcc) to Teensy Vin, pin 14 (GND) to Teensy GND
11. [ ] Key on — ECU should boot normally (3 slow blinks)
12. [ ] If no start: revert to programmed EPROM, check wiring

## Troubleshooting

| Symptom | Check |
|---------|-------|
| 10 fast blinks | No ROMs in flash — upload via USB or insert SD card |
| Rapid continuous blink | Flash init failed — re-flash firmware |
| No serial output | USB cable, Teensy powered? |
| ECU doesn't boot | Verify /OE and /CE wiring, check data bus connections |
| ECU boots but runs rough | Wrong ROM file, or address bus wiring error |
| Works sometimes | Loose breadboard connection, add bypass caps |
