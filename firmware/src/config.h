// =============================================================================
//  config.h — TeensyEprom pin assignments and configuration
//
//  Teensy 4.1 EPROM emulator — replaces 27C256/27C512 in any parallel-bus ECU.
//
//  Hardware:
//    Teensy 4.1 (IMXRT1062, 600MHz, 3.3V GPIO — NOT 5V tolerant)
//    2x 74HCT245 @ 3.3V Vcc — address bus level shift (ECU 5V -> Teensy 3.3V)
//    Data bus direct — Teensy 3.3V outputs meet ECU TTL VIH (>2.0V)
//    /OE, /CE — 1k series resistors (5V -> 3.3V clamp via Teensy internal diodes)
//
//  Power:
//    EPROM socket pin 28 (Vcc) provides 5V from the ECU.
//    Connect to Teensy Vin pin — the onboard regulator drops to 3.3V.
//    This powers the Teensy, SD card, and both 74HCT245s (from 3.3V rail).
//    USB can be connected simultaneously for serial commands.
// =============================================================================
#pragma once

// ---------------------------------------------------------------------------
// ROM
// ---------------------------------------------------------------------------
#define ROM_SIZE              65536    // 27C512 = 64KB (or 27C256 mirrored)
#define ROM_ACTIVE_SIZE       32768    // Most ECUs use 32KB, mirrored to fill 64KB
#define MAX_MAPS              16       // Maximum ROM slots

// ---------------------------------------------------------------------------
// Storage — LittleFS on internal program flash (primary)
// ---------------------------------------------------------------------------
// Teensy 4.1 has 8MB flash. Firmware uses ~200KB. We reserve 1MB at the
// end for LittleFS. 16 x 32KB = 512KB fits with room to spare.
//
// SD card is fallback only — checked at boot if LittleFS has no maps.
// This avoids SD reliability issues (vibration, heat, contact oxidation).
#define LFS_SIZE              (1024 * 1024)   // 1MB for ROM filesystem
#define MAP_DIR               "/maps/"        // directory for ROM files (LittleFS)
#define ACTIVE_FILE           "/active.txt"   // persists active map index

// SD card fallback
#define SD_MAP_DIR            "maps/"         // SD card directory
#define SD_FALLBACK_FILE      "tune.bin"

// ---------------------------------------------------------------------------
// Address bus — 16 lines via 2x 74HCT245 @ 3.3V (ECU 5V -> Teensy 3.3V)
// ---------------------------------------------------------------------------
static const uint8_t ADDR_PINS[16] = {
//  A0  A1  A2  A3  A4  A5  A6  A7      <- U1 B-side
     2,  3,  4,  5,  6,  7,  8,  9,
//  A8  A9 A10 A11 A12 A13 A14 A15      <- U2 B-side
    10, 11, 12, 24, 25, 26, 27, 28
};

// ---------------------------------------------------------------------------
// Data bus — 8 lines direct to DIP-28 (no level shifter needed)
// ---------------------------------------------------------------------------
static const uint8_t DATA_PINS[8] = {
//  D0  D1  D2  D3  D4  D5  D6  D7
    14, 15, 16, 17, 18, 19, 20, 21
};

// ---------------------------------------------------------------------------
// Control signals — 1k series resistors (5V -> 3.3V via internal clamp)
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
#define DEBOUNCE_MS          50
#define LED_BLINK_MS         200
#define BUTTON_HOLD_MS       1000     // Hold for previous map

// ---------------------------------------------------------------------------
// Upload protocol
// ---------------------------------------------------------------------------
// Binary upload with CRC32 verification:
//   Host: UPLOAD <filename> <size>\n
//   Dev:  READY <size>\n
//   Host: <size raw bytes> <4-byte CRC32 big-endian>
//   Dev:  OK <filename> <size> CRC32=XXXXXXXX\n
//      or ERR: <reason>\n
#define UPLOAD_TIMEOUT_MS    10000    // 10s timeout for binary transfer

// ---------------------------------------------------------------------------
// Ident
// ---------------------------------------------------------------------------
#define FW_VERSION           "2.0"
#define IDENT_STRING         "TeensyEprom v" FW_VERSION "\n"
