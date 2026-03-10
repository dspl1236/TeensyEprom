# Audi 90 2.6L Stroker — Teensy 4.1 ECU Interface

[![Build Windows EXE](https://github.com/dspl1236/audi90-teensy-ecu/actions/workflows/build.yml/badge.svg)](https://github.com/dspl1236/audi90-teensy-ecu/actions/workflows/build.yml)
[![Download Latest](https://img.shields.io/github/downloads/dspl1236/audi90-teensy-ecu/latest/total?label=Downloads&color=brightgreen)](https://github.com/dspl1236/audi90-teensy-ecu/releases/tag/latest)

> **Teensy 4.1 EPROM emulator + closed-loop fuel/ignition correction layer for the Hitachi 893906266D ECU (1990 Audi 90 / CQ 2.3 20v and variants)**

---

## ⬇️ Download the Tuner App (Windows)

**[→ Download Audi90Tuner.exe (latest build)](https://github.com/dspl1236/audi90-teensy-ecu/releases/tag/latest)**

No install required. Just download, plug in your Teensy, and run.

---

## What This Does

This project replaces the 27C512 EPROM in a Hitachi 443 906 022 ECU with a **Teensy 4.1** that:

- **Emulates the EPROM in RAM** — serves the original ROM data to the ECU via GPIO interrupt (~70ns response, within 27C512 spec)
- **Intercepts MAF frequency** — scales airflow signal for displacement changes (Phase 1: stock 7A MAF ×1.130 for 2.6L; Phase 2: Bosch 1.8T analog MAF)
- **Intercepts injector pulses** — ±15% pulse width correction via MOSFET gates
- **Closed-loop corrections** — modifies `romData[]` in RAM live using wideband AFR feedback, knock sensor retard, and MAP sensor
- **Datalogs everything** to SD card (CSV, 10Hz)
- **Live USB tuning** via the Windows app — edit fuel/timing maps in real time, view gauges, load ROM files from SD

---

## Hardware

| Part | Purpose |
|------|---------|
| Teensy 4.1 (unlocked) | Main controller — 600MHz, 1MB RAM, USB HS, SD built-in |
| 74HCT245N ×3 | 5V↔3.3V level shifters for ECU bus |
| 28-pin ZIF socket | EPROM socket adapter |
| GM 3-bar MAP sensor #55567257 | Load sensing |
| Bosch knock sensor 0261231006 | Knock detection |
| IRLZ44N MOSFET ×4 | Injector pulse intercept |
| TJA1051T ×2 | CAN bus transceivers |
| Spartan 3 Lite OEM + Bosch LSU ADV | Wideband O2 controller |
| LM2596 + AMS1117-3.3 | Power regulation |

Full BOM with sources: [BOM.md](BOM.md)

---

## Tuner App Features

- **Auto-detect Teensy** by USB Vendor ID — or pick COM port manually
- **Live gauges** — AFR, RPM, MAP, TPS, IAT, Fuel Trim %, Knock retard (10Hz)
- **16×16 Fuel + Timing map editors** with heat-map colouring — edits push to Teensy RAM instantly
- **ROM selector** — load any `.bin` from Teensy SD card
- **Corrections toggle** — enable/disable closed-loop fuel trim live
- **Datalog viewer** — load CSV from SD, plot all channels
- **Serial console** — raw TX/RX log, manual command entry

---

## Wiring Overview

```
Hitachi ECU (27C512 socket)
  A0–A15  →  74HCT245 (3 chips)  →  Teensy GPIO
  D0–D7   →  74HCT245            →  Teensy GPIO
  /OE     →  Teensy D6 (ISR trigger)
  /CE     →  Teensy D8

Teensy ADC inputs:
  ADC1  ←  GM 3-bar MAP (10k/20k divider)
  ADC2  ←  Bosch knock sensor (AC bias)
  ADC3  ←  TPS (tee off ECU signal)
  ADC4  ←  IAT (NTC)

Teensy injector intercept:
  D2–D5  →  IRLZ44N gates  →  injector signal path

MAF intercept:
  D20  ←  Stock 7A MAF (frequency in)
  D21  →  ECU MAF input (corrected frequency out)

Spartan 3 Lite wideband (5V UART):
  SERIAL1 TX/RX  ←→  10k/20k divider  ←→  Spartan TX
```

Full schematic: [schematics/](schematics/)

---

## Firmware Modules

| Module | Purpose |
|--------|---------|
| `eprom_emu.h` | GPIO interrupt EPROM emulator |
| `wideband.cpp` | Spartan 3 Lite UART parser (LSU ADV) |
| `sensors.cpp` | MAP, knock, TPS, IAT, RPM |
| `corrections.cpp` | Closed-loop fuel trim + knock retard |
| `injectors.cpp` | MOSFET pulse intercept |
| `maf.cpp` | Phase 1 freq intercept / Phase 2 1.8T analog |
| `can_datalog.cpp` | CAN broadcast + SD CSV logger |
| `usb_tune.h` | USB serial protocol for tuner app |

### Build (PlatformIO)
```
pio run                    # Normal build
pio run -e debug           # Debug build
pio run --target upload    # Flash to Teensy
```

### SD Card Setup
```
SD:/
├── stock.bin   ← Original EPROM dump (read with T48 programmer)
└── tune.bin    ← Active tune (start as copy of stock.bin)
```

---

## Boot Sequence

```
[1] SD init + ROM load → romData[65536]
[2] ROM validate (checksum)
[3] EPROM emulator arm (GPIO ISR on /OE)
[4] Wideband UART init (Spartan 3 Lite @ 9600 8N1)
[5] Sensors init (MAP, knock, TPS, IAT, RPM)
[6] Injector MOSFET gates init
[7] CAN bus init
[8] SD datalogger init
[9] USB tuner ready
LED: 3 blinks = all OK
```

---

## MAF Strategy

| Phase | Sensor | Method |
|-------|--------|--------|
| **Phase 1** (now) | Stock 7A hotwire MAF | Frequency intercept ×1.130 (2.6÷2.3) |
| **Phase 2** (turbo) | Bosch 0 280 218 037 in VR6/S4 225mm housing | Analog 0–5V → kg/h → 7A freq synthesised |

Switch: `#define MAF_INPUT_TYPE` in `config.h`. No hardware change needed.

---

## Target Vehicle

- **1990 Audi 90 / Coupe Quattro** — 7A/NF block
- **ECU:** Hitachi 443 906 022 — Late 4-connector variant **893906266D**
- **Engine:** 2.6L stroker — AAF Eurovan crank (95.6mm), VW Mk6 CCTA rods/pistons

Compatible with any vehicle running the **893906266D** ECU.

---

## Disclaimer

This project modifies engine management systems. Use at your own risk. Not for road use where prohibited. Always have a backup EPROM. The author accepts no liability for engine damage, emissions violations, or any other consequences.

---

## License

MIT — free to use, modify, and distribute. Credit appreciated but not required.
