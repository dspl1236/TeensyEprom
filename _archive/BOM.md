# Audi 90 Teensy ECU Project — Full BOM
*All parts ordered ~$320 total shipped*

| Ref | Part | Notes |
|-----|------|-------|
| U1 | Teensy 4.1 with pins | Amazon |
| WB1 | Spartan 3 Lite OEM | 14point7.com — specify "configure for LSU ADV" |
| WB2 | Bosch LSU ADV sensor 0258027157 / 17087 | 14point7.com — **5-pin connector** |
| WB3 | M18×1.5 weld-in exhaust bung | Amazon |
| U2–U4 | SN74HCT245N DIP-20 ×10 (Juried Engineering) | Amazon — **NOT 74LS245** |
| R-div | 10kΩ + 20kΩ UART level divider | From resistor kit |
| S1 | GM 3-bar MAP sensor #55567257 w/ pigtail | Amazon |
| S2 | Bosch knock sensor 0261231006 | Amazon |
| Q1–Q4 | IRLZ44N MOSFET ×10 (ALLECIN) | Amazon |
| D1–D4 | 1N4007 flyback diodes ×125 (BOJACK) | Amazon |
| U5–U6 | TJA1051T CAN transceiver modules ×2 | Amazon |
| U7 | LM2596 buck converter modules ×5 | Amazon — set to 5.00V before connecting |
| U8 | AMS1117-3.3 LDO modules ×10 (HiLetgo) | Amazon |
| D5 | P6KE15A TVS diodes ×20 | Amazon |
| D6 | 1N5822 Schottky diodes ×25 (BOJACK) | Amazon |
| J1 | 28-pin ZIF socket ×5 | Amazon |
| PROG | T48 TL866-3G programmer | Amazon |
| C1 | Tantalum 1–100µF 16V assortment (XINGYHENG) | Amazon |
| C2 | 0.1µF ceramic 120pcs (mxuteuk) | Amazon — bypass at every IC Vcc |
| C3 | 100pF ceramic 100pcs (Cermant) | Amazon |
| C4 | 100µF 25V Rubycon electrolytic | Amazon |
| C5 | 470µF 25V Rubycon electrolytic | Amazon — on 12V rail |
| R-KIT | 600pc resistor kit 1% metal film (VIBICCK) | Amazon |
| BB | BOJACK breadboard kit 4×830pt + jumpers | Amazon |
| FUSE | 4-pack 12AWG inline fuse holders (Cooclensportey) | Amazon |
| PCB | JLCPCB 2-layer custom | After prototype proven |

## Critical Notes
- **74HCT245N not 74LS245** — wrong input thresholds on LS variant
- **LSU ADV is 5-pin** — LSU 4.9 is 6-pin, do not confuse
- **Spartan heater GND (Pin 13) MUST be connected before power-up** or enters bootloader
- **LM2596 must be set to 5.00V on bench before connecting to circuit**
- **470µF caps on 12V rail** — use 25V or 35V rated
