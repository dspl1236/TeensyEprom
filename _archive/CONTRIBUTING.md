# Contributing to Audi90 Teensy ECU

Thanks for your interest in contributing! This project is free and open — built by an Audi enthusiast for the community. All skill levels welcome.

---

## Ways to Contribute

### 🔧 Hardware / Wiring
- Test on different ECU variants (893906266B, other Hitachi units)
- Verify connector pinouts on RHD / export market cars
- Report differences in MAF sensor wiring by market/year
- PCB layout contributions (KiCad preferred)

### 💻 Firmware
- Additional sensor support (flex fuel, oil pressure, EGT)
- Improved knock detection algorithm
- FlexIO DMA EPROM emulation (Phase 2 — zero CPU overhead)
- CAN broadcast expansion (custom gauge heads, dash displays)
- Support for other ECU variants (map address changes in `config.h`)

### 🖥 Tuner App
- macOS / Linux support (app is pure Python — should work, untested)
- VE table editor (volumetric efficiency based tuning)
- 3D map visualisation
- Live AFR target map (lambda by RPM/load cell)
- Closed-loop PID tuning controls
- Import/export TunerPro XDF compatibility

### 📖 Documentation
- Wiring photos from real installs
- Step-by-step breadboard build guides
- Video walkthroughs
- Translations

### 🐛 Bug Reports
Open an issue with:
- Your ECU part number
- Engine/car details
- What happened vs what you expected
- Any serial console output from the tuner app

---

## Getting Started

```bash
# Clone the repo
git clone https://github.com/dspl1236/audi90-teensy-ecu.git
cd audi90-teensy-ecu

# Firmware — requires PlatformIO
cd firmware
pio run

# Tuner app — requires Python 3.9+
cd tuner_app
pip install PyQt5 pyserial matplotlib
python main.py
```

---

## Pull Request Guidelines

1. **One change per PR** — keeps review simple
2. **Test before submitting** — if it's firmware, test on actual hardware if possible
3. **Update docs** — if you change the protocol or wiring, update README and comments
4. **Keep the protocol stable** — `usb_tune.h` and `serial_comm/protocol.py` must stay in sync. If you change the protocol, bump both files and note it clearly in the PR description
5. **No breaking changes without discussion** — open an issue first for large changes

---

## Project Structure

```
audi90-teensy-ecu/
├── firmware/
│   └── src/
│       ├── main.cpp          # Boot sequence, main loop
│       ├── config.h          # All pin assignments and constants
│       ├── eprom_emu.h       # EPROM emulator (GPIO interrupt)
│       ├── wideband.cpp      # Spartan 3 Lite UART parser
│       ├── sensors.cpp       # MAP, knock, TPS, IAT, RPM
│       ├── corrections.cpp   # Closed-loop fuel/ignition corrections
│       ├── injectors.cpp     # MOSFET injector intercept
│       ├── maf.cpp           # MAF frequency intercept (Phase 1/2)
│       ├── can_datalog.cpp   # CAN + SD datalogger
│       └── usb_tune.h        # USB tuner protocol
├── tuner_app/
│   ├── main.py               # Entry point
│   ├── serial_comm/
│   │   └── protocol.py       # USB serial protocol (must match usb_tune.h)
│   └── ui/
│       ├── connection_panel.py
│       ├── gauges_tab.py
│       ├── map_editor_tab.py
│       ├── rom_tab.py
│       ├── datalog_tab.py
│       ├── console_tab.py
│       └── main_window.py
├── schematics/               # HTML interactive schematics
├── rom_files/                # Stock ROM dumps and reference tunes
└── .github/workflows/        # Auto-build Windows EXE on push
```

---

## Tested On

| Car | ECU | Status |
|-----|-----|--------|
| 1990 Audi 90 2.3 20v (7A) stroked to 2.6L | 893906266D | ✅ In development |

If you get it running on another variant, please open a PR to add your car to this table!

---

## Questions?

Open a GitHub Issue — no question is too basic. This is a community project.
