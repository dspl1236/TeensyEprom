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
| 28 | Vcc | **Leave unconnected** or 3.3V — see note |

> **DIP-28 Pin 28 (Vcc):** The original EPROM runs at 5V. The Teensy is
> powered separately via USB or its own regulator. Connect pin 28 to 3.3V
> ONLY if U1/U2 need a local Vcc tie point. Do NOT connect to 5V — it
> would back-feed into the 3.3V rail through the 74HCT245 protection diodes.

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

## SD Card Setup

Format micro SD as **FAT32**. Two options for ROM files:

**Option A — maps/ folder (recommended for multiple maps):**
```
SD:/
└── maps/
    ├── 01_stock.bin
    ├── 02_stage1.bin
    └── 03_stage2.bin
```

**Option B — root (simple, single/dual map):**
```
SD:/
├── tune.bin     ← loaded first
└── stock.bin    ← fallback
```

Files must be 32KB (32,768 bytes) or 64KB (65,536 bytes).
32KB files are automatically mirrored into both halves (A15-agnostic).

## Firmware Upload

```bash
cd firmware
pio run -c platformio_lite.ini -e teensy_lite --target upload
```

Or use Arduino IDE with Teensy 4.1 board selected, open
`firmware/src/lite/main_lite.cpp`.

## USB Serial Commands

Connect via serial monitor at 115200 baud:

| Command | Description |
|---------|-------------|
| `INFO` | Firmware version, active map, status |
| `LIST` | List all maps (* = active) |
| `MAP 2` | Switch to map index 2 |
| `DUMP` | Print first 16 bytes of active ROM |

## LED Status

| Pattern | Meaning |
|---------|---------|
| 3 slow blinks | Boot OK, emulation active |
| N blinks | Switched to map N |
| Rapid blink | Fatal error (no SD / no ROM files) |

## First Power-On Checklist

1. [ ] Verify all wiring against tables above (twice)
2. [ ] SD card inserted with at least one .bin file
3. [ ] DIP-28 socket NOT installed in ECU yet
4. [ ] Connect Teensy via USB
5. [ ] Open serial monitor — confirm "EPROM active"
6. [ ] Verify ROM loaded: type `DUMP` — should show non-zero data
7. [ ] Measure voltages: 3.3V on U1/U2 Vcc, GND on DIR and /OE pins
8. [ ] Install DIP-28 socket in ECU, connect ribbon cable
9. [ ] Key on — ECU should boot normally
10. [ ] If no start: revert to programmed EPROM, check wiring

## Troubleshooting

| Symptom | Check |
|---------|-------|
| Rapid LED blink | SD card missing or no .bin files found |
| No serial output | USB cable, Teensy powered? |
| ECU doesn't boot | Verify /OE and /CE wiring, check data bus connections |
| ECU boots but runs rough | Wrong ROM file, or address bus wiring error |
| Works sometimes | Loose breadboard connection, add bypass caps |
