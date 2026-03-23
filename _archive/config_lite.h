// =============================================================================
//  config_lite.h — Pin assignments for Teensy Lite (EPROM emulator only)
//
//  Hardware:
//    Teensy 4.1 (IMXRT1062, 600MHz, 3.3V GPIO — NOT 5V tolerant)
//    2× 74HCT245 @ 3.3V Vcc — address bus level shift (ECU 5V → Teensy 3.3V)
//    Data bus direct — Teensy 3.3V outputs meet ECU TTL VIH (>2.0V)
//    /OE, /CE — 1kΩ series resistors (5V → 3.3V clamp via Teensy internal diodes)
//
//  Breadboard layout:
//    Left side  = address bus (via 74HCT245) + control signals
//    Right side = data bus (direct to DIP-28) + LED
// =============================================================================
#pragma once

// ---------------------------------------------------------------------------
// ROM
// ---------------------------------------------------------------------------
#define ROM_SIZE              65536    // 27C512 = 64KB
#define ROM_ACTIVE_SIZE       32768    // 7A/AAH ECU uses 32KB, mirrored
#define ROM_FILENAME          "tune.bin"
#define ROM_FALLBACK_FILENAME "stock.bin"
#define MAP_DIR               "maps/"  // SD card directory for multiple maps

// ---------------------------------------------------------------------------
// Address bus — 16 lines via 2× 74HCT245 @ 3.3V (ECU 5V → Teensy 3.3V)
//
// U1 (address low byte, 74HCT245 #1):
//   Vcc=3.3V, DIR=LOW (A→B), /OE=LOW (always enabled)
//   A-side: DIP-28 socket A0–A7 (ECU 5V)
//   B-side: Teensy pins 2–9 (3.3V)
//
// U2 (address high byte, 74HCT245 #2):
//   Vcc=3.3V, DIR=LOW (A→B), /OE=LOW (always enabled)
//   A-side: DIP-28 socket A8–A15 (ECU 5V)
//   B-side: Teensy pins 10–12, 24–28 (3.3V)
// ---------------------------------------------------------------------------
static const uint8_t ADDR_PINS[16] = {
//  A0  A1  A2  A3  A4  A5  A6  A7      ← U1 B-side
     2,  3,  4,  5,  6,  7,  8,  9,
//  A8  A9 A10 A11 A12 A13 A14 A15      ← U2 B-side
    10, 11, 12, 24, 25, 26, 27, 28
};

// ---------------------------------------------------------------------------
// Data bus — 8 lines direct to DIP-28 (no level shifter needed)
//
// Teensy 3.3V output → ECU TTL input (VIH > 2.0V): reads as HIGH ✓
// Teensy controls direction: OUTPUT when serving data, INPUT (tri-state)
// when ECU is accessing other bus devices (RAM, I/O).
// ---------------------------------------------------------------------------
static const uint8_t DATA_PINS[8] = {
//  D0  D1  D2  D3  D4  D5  D6  D7
    14, 15, 16, 17, 18, 19, 20, 21
};

// ---------------------------------------------------------------------------
// Control signals — 1kΩ series resistors (5V → 3.3V via internal clamp)
//
// ECU drives /OE and /CE at 5V. 1kΩ series resistor limits clamp current
// to (5.0 − 3.3) / 1000 = 1.7 mA — well within Teensy spec.
// ---------------------------------------------------------------------------
static const uint8_t PIN_OE = 29;     // /OE (output enable, active LOW)
static const uint8_t PIN_CE = 30;     // /CE (chip enable, active LOW)

// ---------------------------------------------------------------------------
// Map switcher
// ---------------------------------------------------------------------------
static const uint8_t PIN_BUTTON = 31;  // Momentary switch, active LOW (internal pull-up)
static const uint8_t PIN_LED    = 13;  // Onboard Teensy LED

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
// HD6303 bus cycle: ~500ns at 2MHz. Teensy ISR response: ~200ns at 600MHz.
// Margin: 300ns. Well within spec for GPIO bit-bang approach.
// 27C512-120 tACC = 120ns — Teensy ISR is slower than real EPROM but
// ECU latches data at end of bus cycle, not at tACC.
#define DEBOUNCE_MS          50
#define LED_BLINK_MS         200
#define BUTTON_HOLD_MS       1000     // Hold for previous map

// ---------------------------------------------------------------------------
// Ident
// ---------------------------------------------------------------------------
#define IDENT_STRING         "TeensyEprom Lite v1.0\n"
