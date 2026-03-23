# TeensyEprom — Build Guide

> **⚠ Work in Progress — Use at Your Own Risk**
>
> This is a DIY EPROM emulator. Verify every connection before powering on.
> Always keep a known-good programmed EPROM as backup.

## What It Does

Replaces the 27C256 / 27C512 EPROM in Hitachi MMS ECUs with a Teensy 4.1
that serves ROM data from internal flash. Multiple map files can be stored
and switched with a button press or USB serial command.

**Compatible ECUs:** 893906266D (7A 20v), 893906266B (7A early),
4A0906266 (AAH V6), 8A0906266A (MMS-200), 8A0906266B (MMS-300),
and any ECU using a DIP-28 27C256 or 27C512 with parallel bus.

## BOM

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
| — | Micro SD card (FAT32) | 1 | Optional — fallback storage |
| — | Ribbon cable or hookup wire | ~30 | DIP socket → breadboard |

**Total cost:** ~$35 (Teensy) + ~$5 (parts) = **~$40**

## ROM Storage

**Primary: Internal Flash (LittleFS)**
- ROMs stored on Teensy's 8MB program flash — no SD card needed
- Upload/manage via USB serial from desktop app
- Capacity: 16 maps × 32KB = 512KB (1MB partition allocated)
- No moving parts, no contacts to corrode, survives vibration and heat

**Fallback: SD Card**
- If internal flash has no maps at boot, checks SD card
- Useful for initial setup or field recovery
- Copy .bin files to `maps/` folder on FAT32 SD card

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
 │ Vcc ───────────┼── 5V ─────────────────────────────→┼── Vin        │
 └────────────────┘                                     └──────────────┘
```

### Level Shifting

The Teensy 4.1 is **3.3V only — NOT 5V tolerant**. The ECU bus is 5V TTL.

| Signal | Direction | Method |
|--------|-----------|--------|
| A0-A15 | ECU → Teensy | 74HCT245 @ 3.3V Vcc (steps 5V down) |
| D0-D7 | Teensy → ECU | Direct (3.3V meets TTL VIH > 2.0V) |
| /OE, /CE | ECU → Teensy | 1kΩ series resistor (clamp via internal diodes) |

### Power

```
DIP-28 pin 28 (Vcc, 5V from ECU) → Teensy Vin
DIP-28 pin 14 (GND)              → Teensy GND
```

Teensy's onboard regulator drops 5V to 3.3V for the MCU and 74HCT245s.
USB can be connected simultaneously for serial commands.

### Complete Pin-to-Pin Connection Table

Wire everything from this table. Check off each row as you go.

**U1 — 74HCT245 Address Low Byte (DIP-20):**

| U1 pin | U1 function | Wire to            | Signal |
|--------|-------------|---------------------|--------|
| 1      | DIR         | GND                 | A→B direction |
| 2      | A1 (input)  | DIP-28 pin 10       | A0 (5V) |
| 3      | A2 (input)  | DIP-28 pin 9        | A1 (5V) |
| 4      | A3 (input)  | DIP-28 pin 8        | A2 (5V) |
| 5      | A4 (input)  | DIP-28 pin 7        | A3 (5V) |
| 6      | A5 (input)  | DIP-28 pin 6        | A4 (5V) |
| 7      | A6 (input)  | DIP-28 pin 5        | A5 (5V) |
| 8      | A7 (input)  | DIP-28 pin 4        | A6 (5V) |
| 9      | A8 (input)  | DIP-28 pin 3        | A7 (5V) |
| 10     | GND         | GND                 | |
| 11     | B8 (output) | Teensy pin 9        | A7 (3.3V) |
| 12     | B7 (output) | Teensy pin 8        | A6 (3.3V) |
| 13     | B6 (output) | Teensy pin 7        | A5 (3.3V) |
| 14     | B5 (output) | Teensy pin 6        | A4 (3.3V) |
| 15     | B4 (output) | Teensy pin 5        | A3 (3.3V) |
| 16     | B3 (output) | Teensy pin 4        | A2 (3.3V) |
| 17     | B2 (output) | Teensy pin 3        | A1 (3.3V) |
| 18     | B1 (output) | Teensy pin 2        | A0 (3.3V) |
| 19     | /OE         | GND                 | Always enabled |
| 20     | Vcc         | Teensy 3.3V pin     | 3.3V power |

**U2 — 74HCT245 Address High Byte (DIP-20):**

| U2 pin | U2 function | Wire to            | Signal |
|--------|-------------|---------------------|--------|
| 1      | DIR         | GND                 | A→B direction |
| 2      | A1 (input)  | DIP-28 pin 25       | A8 (5V) |
| 3      | A2 (input)  | DIP-28 pin 24       | A9 (5V) |
| 4      | A3 (input)  | DIP-28 pin 21       | A10 (5V) |
| 5      | A4 (input)  | DIP-28 pin 23       | A11 (5V) |
| 6      | A5 (input)  | DIP-28 pin 2        | A12 (5V) |
| 7      | A6 (input)  | DIP-28 pin 26       | A13 (5V) |
| 8      | A7 (input)  | DIP-28 pin 27       | A14 (5V) |
| 9      | A8 (input)  | DIP-28 pin 1        | A15 (5V) |
| 10     | GND         | GND                 | |
| 11     | B8 (output) | Teensy pin 28       | A15 (3.3V) |
| 12     | B7 (output) | Teensy pin 27       | A14 (3.3V) |
| 13     | B6 (output) | Teensy pin 26       | A13 (3.3V) |
| 14     | B5 (output) | Teensy pin 25       | A12 (3.3V) |
| 15     | B4 (output) | Teensy pin 24       | A11 (3.3V) |
| 16     | B3 (output) | Teensy pin 12       | A10 (3.3V) |
| 17     | B2 (output) | Teensy pin 11       | A9 (3.3V) |
| 18     | B1 (output) | Teensy pin 10       | A8 (3.3V) |
| 19     | /OE         | GND                 | Always enabled |
| 20     | Vcc         | Teensy 3.3V pin     | 3.3V power |

**Data Bus — Direct (no buffer):**

| DIP-28 pin | Signal | Teensy pin |
|------------|--------|------------|
| 11         | D0     | 14         |
| 12         | D1     | 15         |
| 13         | D2     | 16         |
| 15         | D3     | 17         |
| 16         | D4     | 18         |
| 17         | D5     | 19         |
| 18         | D6     | 20         |
| 19         | D7     | 21         |

**Control Signals — via 1kΩ resistor:**

| DIP-28 pin | Signal | Resistor | Teensy pin |
|------------|--------|----------|------------|
| 22         | /OE    | R1 1kΩ   | 29         |
| 20         | /CE    | R2 1kΩ   | 30         |

**Power:**

| From | To | Notes |
|------|----|-------|
| DIP-28 pin 28 (Vcc) | Teensy Vin | 5V from ECU powers everything |
| DIP-28 pin 14 (GND) | Teensy GND | Common ground |
| Teensy 3.3V pin | U1 pin 20, U2 pin 20 | 3.3V rail for both 245s |
| DIP-28 pin 1 (Vpp) | DIP-28 pin 28 (Vcc) | Tie Vpp to Vcc (or leave to U2 — it's A15) |

**Button (optional):**

| From | To | Notes |
|------|----|-------|
| Teensy pin 31 | SW1 terminal 1 | Internal pull-up enabled in firmware |
| SW1 terminal 2 | GND | Short press = next map, long press = previous |

### Bypass Caps

Place 0.1µF ceramic caps as close as possible to each IC's power pins:

| Cap | Between | Near |
|-----|---------|------|
| C1  | U1 pin 20 (Vcc) and U1 pin 10 (GND) | U1 |
| C2  | U2 pin 20 (Vcc) and U2 pin 10 (GND) | U2 |
| C3  | Teensy 3.3V and Teensy GND | Near Teensy |

## DIP-28 Pinout (27C256 / 27C512)

```
              ┌────U────┐
       Vpp  1 │         │ 28  Vcc ──→ Teensy Vin
       A12  2 │         │ 27  /PGM (A14 on 512)
        A7  3 │         │ 26  A13 (NC on 256)
        A6  4 │         │ 25  A8
        A5  5 │         │ 24  A9
        A4  6 │         │ 23  A11
        A3  7 │         │ 22  /OE ──→ 1kΩ ──→ Teensy pin 29
        A2  8 │ 27C512  │ 21  A10
        A1  9 │         │ 20  /CE ──→ 1kΩ ──→ Teensy pin 30
        A0 10 │         │ 19  D7
        D0 11 │         │ 18  D6
        D1 12 │         │ 17  D5
        D2 13 │         │ 16  D4
       GND 14 │         │ 15  D3
              └─────────┘
```

**Pin 1 (Vpp):** Tie to Vcc (5V) — programming voltage, not needed for reads.
On 27C512, pin 1 is A15. Teensy serves both halves via mirroring, so A15
state doesn't matter.

**Pin 27:** On 27C256 = /PGM (tie HIGH). On 27C512 = A14 (connect to U2).

## Firmware

### Build & Flash

```bash
cd firmware
pio run                      # compile
pio run --target upload      # flash to Teensy via USB
```

### Boot Sequence

1. Init GPIO, LED off
2. Mount LittleFS (1MB partition on program flash)
3. Try mounting SD card (optional)
4. Scan for .bin files: LittleFS first, SD fallback
5. Load first map into RAM buffer (32KB, mirrored to 64KB)
6. Attach /OE interrupt — EPROM emulation active
7. LED: 3 slow blinks = running

If no maps found: 5 fast blinks, waits for USB upload.

### USB Serial Commands

Connect at 115200 baud:

```
INFO              — firmware version, storage status, active map
LIST              — show all maps (* = active)
MAP <n>           — switch to map index n
DUMP              — hex dump first 256 bytes of active ROM
UPLOAD name 32768 — upload ROM (followed by raw bytes + CRC16)
DOWNLOAD <n>      — download map n (sends SIZE + raw bytes + CRC16)
DELETE <n>        — delete map n from flash
FORMAT            — delete all maps from flash
SCAN              — rescan map files
```

## Desktop App

```bash
pip install pyserial

python app/teensy_eprom.py                     # interactive mode
python app/teensy_eprom.py info                # device info
python app/teensy_eprom.py list                # list maps
python app/teensy_eprom.py upload tune.bin     # upload ROM file
python app/teensy_eprom.py switch 2            # switch to map 2
python app/teensy_eprom.py download 0 out.bin  # download map 0
python app/teensy_eprom.py delete 1            # delete map 1
python app/teensy_eprom.py --port COM3 info    # specify port
```

The app auto-detects the Teensy USB serial port. Use `--port` to override.

## Workflow

1. **Build the hardware** — wire up per circuit diagram above
2. **Flash firmware** — `pio run --target upload`
3. **Upload your first ROM** — `python app/teensy_eprom.py upload stock.bin`
4. **Install** — plug DIP-28 socket into ECU where the EPROM was
5. **Key on** — 3 slow LED blinks = EPROM emulation active
6. **Upload more maps** — `python app/teensy_eprom.py upload stage2.bin`
7. **Switch maps** — button press or `python app/teensy_eprom.py switch 1`

No SD card, no chip burner, no socket swapping. Create ROM files with
[HachiROM](https://github.com/dspl1236/HachiROM) or
[M35Tool](https://github.com/dspl1236/M35Tool).
