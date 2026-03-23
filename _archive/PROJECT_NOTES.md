# Audi 90 2.6L Stroker — Teensy 4.1 ECU Interface Project
## Project Notes & Reference Document
*Last updated: March 2026*

---

## Engine Build
- **Car:** 1990 Audi 90, originally 2.3L 20v (7A/NF)
- **Stroker:** AAF Eurovan crank (95.6mm stroke) → 2.6L
- **Rods/Pistons:** VW Mk6 CCTA, modified for valve clearance
- **Head gasket:** Currently 2× 7A head gaskets (temporary) — proper metal HG TBD
- **ECU:** Hitachi 443 906 022 — confirmed Late 4-Connector **893906266D**

---

## Project Goal
Replace EPROM in Hitachi ECU with Teensy 4.1 (EPROM emulation via FlexIO parallel bus).
Add closed-loop fuel/ignition correction using wideband O2, MAP sensor, knock sensor.
Datalog everything. Live tune via USB. CAN bus output. Future: custom PCB kit.

---

## Architecture

```
Hitachi ECU (5V) → 74HCT245 ×3 level shifters → Teensy 4.1 (3.3V)
                                                      ├── FlexIO: EPROM emulation (A0–A16, D0–D7, /OE, /CE)
                                                      ├── SERIAL1 pins 0/1: Spartan 3 Lite OEM UART (9600 8N1)
                                                      ├── ADC1: GM 3-bar MAP (10k/20k voltage divider)
                                                      ├── ADC2: Bosch knock sensor (AC bias circuit)
                                                      ├── ADC3: TPS (tee off, voltage divider)
                                                      ├── ADC4: IAT sensor
                                                      ├── D2–D5: IRLZ44N MOSFET injector intercept gates
                                                      ├── D6: MAF frequency signal synthesis output
                                                      ├── D7: RPM/coil trigger input (interrupt)
                                                      ├── CAN1/CAN2: TJA1051 transceivers → OBD2, gauge
                                                      ├── USB HS: live tuning interface
                                                      └── SD card: datalogger (built-in)
```

---

## Key Design Decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| MCU | Teensy 4.1 | 600MHz M7, FlexIO, 18× ADC, 3× CAN FD, SD, USB HS |
| Wideband | Spartan 3 Lite OEM + LSU ADV | UART interface, 20× resolution vs LSU 4.9 |
| Level shifters | 74HCT245N (NOT 74LS245) | HCT variant essential for 3.3V↔5V |
| MAF (now) | Stock 7A hotwire, intercepted | Keep variables low during dev |
| MAF (turbo) | Bosch 0 280 218 116 (B5 S4 hot film) | Backflow-immune, 480+ g/s range, built-in IAT |

---

## Wideband — Spartan 3 Lite OEM

**Sensor:** Bosch LSU ADV (5-pin connector) — part 0 258 027 157 / 17087
**Order from:** 14point7.com — specify "configure for LSU ADV" in notes
**One-time setup:** Send `SETTYPE1` via serial at 9600 baud to enable LSU ADV mode

### Pinout (15-pin header)
| Pin | Function |
|-----|----------|
| 2 | LSU H+ (Grey) |
| 3 | LSU IP (Red) |
| 4 | LSU UN (Black) |
| 5 | LSU H- (White) |
| 6 | LSU VM (Yellow) |
| 8 | Linear Output (0V@10AFR–5V@20AFR) |
| 10/14 | E Ground (electronics, 100mA max) |
| 11 | UART Rx (← Teensy TX pin 1, direct) |
| 12 | UART Tx (→ Teensy RX pin 0, via 10k/20k divider) |
| 13 | **Heater GND (3A, MUST connect before power-up)** |
| 15 | 12V supply (5A fuse) |

### UART Serial Format
Send ASCII `G` to start stream. Response:
```
0:a:14.7    ← AFR
1:a:780     ← LSU temp °C
2:a:3000
3:a:129
```

### UART Voltage Divider
Spartan TX (5V) → 10kΩ → Teensy RX (pin 0); 20kΩ junction to GND
Teensy TX (3.3V) → Spartan RX direct (no divider needed)

---

## ROM Analysis — 893906266D

**ROM structure:** 64KB chip, 32KB active (0x0000–0x7FFF), mirrored to 0x8000–0xFFFF

### Confirmed Map Addresses
| Table | Address | Size |
|-------|---------|------|
| Fuel Map (primary 16×16) | 0x0000–0x00FF | 256 bytes |
| Fuel sub-tables | 0x0100–0x01FF | 256 bytes |
| Unknown (injector timing?) | 0x0255–0x025F | 11 bytes |
| Fuel/timing blend | 0x02D1–0x034F | 127 bytes |
| MAF/load scaling | 0x0653–0x0677 | 37 bytes |
| Scalars | 0x077E, 0x07E1 | 1 byte each |
| Timing Map (primary 16×16) | 0x1000–0x10FF | 256 bytes |
| Timing scalars | 0x13F7–0x13FB | 5 bytes |
| Injector scaler | 0x1600–0x16AE | 175 bytes |

### Injector Scaler Values
| Map | Value | Meaning |
|-----|-------|---------|
| Stock 266D | 0x00/0x04 | Stock 7A injectors |
| Stage 1 266B | 0x04 | **Same as stock** — no injector upgrade |
| Turbo Stage 2 550cc | 0xFF | 550cc injectors |

---

## Power Supply

| Rail | Method | Notes |
|------|--------|-------|
| 12V input | P6KE15A TVS + 1N5822 + 470µF 25V | Spike clamp + reverse polarity |
| 5V / 3A | LM2596 pre-built module | Set to 5.00V before connecting |
| 3.3V | AMS1117-3.3 LDO from 5V | Teensy VIN |
| Spartan 12V | Separate switched IGN + 5A fuse | Dedicated GND to battery − only |

---

## Breadboard Build Sequence

| Stage | Task | Gate |
|-------|------|------|
| 0 | EPROM dump with T48 programmer | Save binary to 2+ locations |
| 1 | Power bench test — LM2596→5V, AMS1117→3.3V | Teensy USB enumerates |
| 2 | Spartan UART bench test — lambda stream at 9600 | Confirm AFR values |
| 3 | Level shifters + ZIF + FlexIO timing | Logic analyzer (DSLogic Plus — pending) |
| 4 | Sensor inputs — MAP ADC, knock bias, TPS, IAT | Verify voltages |
| 5 | Injector MOSFET bench test | Scope gate switching |
| 6 | First ECU connection (engine off, fuel pump relay out) | Logic analyzer confirms bus |
| 7 | First start — stock map served, no corrections | Engine runs |
| 8 | Wideband + closed loop — ±10% trim | Datalog first |
| 9 | PCB design — KiCad → JLCPCB 2-layer ENIG | After breadboard proven |

---

## MAF Notes

### Stock 7A MAF (Hitachi hotwire)
- Bore: 50mm main, 8mm bypass
- 4-pin: Signal / GND / Supply / CO pot
- Range: 480 kg/h per German site research
- **German site finding:** Big AAH V6 MAF swap is pointless on NA 7A — flow bench gains are negated by system turbulence. Stock unit is actually 3% better in full system.

### V6 AAH MAF (3-wire, on shelf)
- Bore: 74mm main, 10mm bypass  
- 3-pin: Signal / GND / Supply (no CO pot)
- **Same transfer curve as stock 7A unit**
- Missing CO pot → fault code 00521 → fix with 20kΩ pot + 1kΩ resistor on Pin 4
- Teensy DAC can drive Pin 4 directly for software CO trim

### Future Turbo MAF
- **Bosch 0 280 218 116** (B5 S4/RS4 2.7T hot film)
- Hot film = backflow immune under boost
- Known transfer curve (s4wiki documented)
- ~480–500 g/s saturation point
- Built-in IAT sensor (bonus ADC input)
- Teensy translates S4 curve → 7A Hitachi curve for ECU

---

## Future Plans
- Turbo in 1–2 years (K24 or similar)
- Cone filter replacing airbox at turbo time
- MAF pre-turbo placement, 10× diameter straight section required
- Bosch S4 hot film MAF at turbo install — firmware input cal table swap only
- Proper metal head gasket (specs TBD)
- Custom PCB kit for other builders after breadboard proven

---

## Pending Items
- [ ] EPROM dump (T48 programmer — parts not yet arrived)
- [ ] FlexIO EPROM emulator firmware
- [ ] Binary diff deeper analysis — cross-ref with .ecu Java object for exact table names
- [ ] Verify MAF signal type with oscilloscope (frequency vs analog)
- [ ] Check if Bosch knock sensor 0261231006 already present on 7A block
- [ ] Head gasket specs for 2.6L stroker deck height
- [ ] Logic analyzer (DSLogic Plus — budget pending)

