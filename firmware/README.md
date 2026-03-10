# Firmware

Teensy 4.1 firmware for the Audi 90 ECU interface.

## Structure

```
firmware/
├── platformio.ini          # Build configuration
└── src/
    ├── main.cpp            # Boot sequence + main loop
    ├── config.h            # Pin assignments + constants
    ├── eprom_emu.h         # FlexIO EPROM emulator (header + impl)
    ├── wideband.cpp        # Spartan 3 Lite OEM UART parser
    ├── corrections.cpp     # Closed-loop fuel trim + knock retard
    └── modules.h           # Stubs: sensors, injectors, CAN, datalog
```

## Build

Install [PlatformIO](https://platformio.org/) then:

```bash
cd firmware
pio run                        # Build
pio run --target upload        # Build + upload to Teensy
pio device monitor             # Serial monitor (115200 baud)
```

## SD Card Setup

Format SD card as FAT32. Place ROM files in root:

```
SD:/
├── stock.bin    ← Stock 893906266D ROM dump (from T48 programmer)
└── tune.bin     ← Active tune (copy of stock to start)
```

The firmware loads `tune.bin` first, falls back to `stock.bin`.

## Boot Sequence

1. SD card init + ROM load into RAM (64KB)
2. ROM mirror: lower 32KB copied to upper 32KB
3. ROM validation (checks for blank/erased)
4. FlexIO EPROM emulator armed (GPIO interrupt on /OE)
5. Spartan 3 wideband UART started (9600 8N1, sends 'G')
6. Sensors initialized (MAP, knock, TPS, IAT)
7. Injector intercept MOSFETs initialized
8. CAN transceivers initialized
9. Datalogger initialized
10. LED: 3 slow blinks = OK, rapid blink = fatal error

## EPROM Emulator

**Phase 1 (current): GPIO interrupt mode**
- /OE falling edge → ISR reads A0–A15, serves D0–D7 from romData[]
- Response time: ~60–80ns at 600MHz (within 27C512 tACC = 120ns spec)
- CPU overhead: moderate (every EPROM read triggers ISR)

**Phase 2 (planned): FlexIO DMA mode**
- Address bus captured by FlexIO shifter
- Data driven by FlexIO timer + DMA
- Zero CPU overhead during EPROM reads

## Corrections

The corrections engine modifies `romData[]` in RAM in real time.
The EPROM emulator always serves current `romData[]` contents.

- **Fuel trim**: ±10% max, step 0.2%/cycle, requires valid wideband signal
- **Knock retard**: 2°/knock event, max 10°, recovers 0.5°/cycle (no knock)

Corrections apply only to the **current operating cell** (MAP × RPM).
Other cells are not touched — only cells actually visited get corrected.

## Known TODOs

- [ ] Calibrate timing map: determine counts-per-degree for 893906266D
- [ ] Implement sensors.cpp (MAP ADC scaling, knock peak detection, IAT NTC table)
- [ ] Implement injectors.cpp (pulse width measurement + MOSFET gate control)
- [ ] Implement can_bus.cpp (FlexCAN_T4 broadcast)
- [ ] Implement datalog.cpp (SD CSV write at 10Hz)
- [ ] MAF frequency intercept (PIN_MAF_IN → correction → PIN_MAF_OUT)
- [ ] FlexIO DMA EPROM emulator (Phase 2)
- [ ] USB live tune protocol (read/write romData[] cells over USB serial)
