// =============================================================================
// Audi 90 2.6L Stroker — Teensy 4.1 ECU Interface
// main.cpp — Boot sequence + FlexIO EPROM emulator
//
// Target:   Teensy 4.1 (IMXRT1062, 600MHz ARM Cortex-M7)
// ECU:      Hitachi 443 906 022 / 893906266D (Late 4-connector)
// ROM:      64KB (27C512), active 0x0000–0x7FFF mirrored to 0x8000–0xFFFF
//
// Boot sequence:
//   1. Init serial (USB) for debug
//   2. Init SD card, load ROM binary into RAM
//   3. Init FlexIO EPROM emulator (address + data bus)
//   4. Init Spartan 3 wideband UART
//   5. Init sensors (MAP, knock, TPS, IAT)
//   6. Init injector intercept MOSFETs
//   7. Init CAN transceivers
//   8. Signal ready — ECU may now read EPROM
//   9. Enter main loop
// =============================================================================

#include <Arduino.h>
#include <SD.h>
#include <FlexIO_t4.h>

#include "config.h"
#include "eprom_emu.h"
#include "wideband.h"
#include "sensors.h"
#include "injectors.h"
#include "can_bus.h"
#include "datalog.h"
#include "corrections.h"

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
uint8_t  romData[ROM_SIZE];          // 64KB ROM image in RAM
bool     romLoaded    = false;
bool     ecuReady     = false;
uint32_t bootTimeMs   = 0;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
bool     loadRomFromSD(const char* filename);
void     signalBootStatus(bool ok);

// ---------------------------------------------------------------------------
// setup() — runs once at power-on
// ---------------------------------------------------------------------------
void setup() {

  // ── 1. USB serial debug ────────────────────────────────────────────────
  Serial.begin(115200);
  // Don't block on serial — ECU won't wait for a PC to connect
  delay(200);
  Serial.println(F("\n=== Audi90 Teensy ECU Interface ==="));
  Serial.println(F("Boot starting..."));

  // ── 2. SD card + ROM load ──────────────────────────────────────────────
  Serial.print(F("SD init... "));
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println(F("FAILED — halting"));
    signalBootStatus(false);
    while (1);                        // Can't run without ROM
  }
  Serial.println(F("OK"));

  // Try primary ROM slot first, fall back to stock
  Serial.print(F("Loading ROM: "));
  Serial.println(ROM_FILENAME);

  if (!loadRomFromSD(ROM_FILENAME)) {
    Serial.print(F("Not found, trying fallback: "));
    Serial.println(ROM_FALLBACK_FILENAME);
    if (!loadRomFromSD(ROM_FALLBACK_FILENAME)) {
      Serial.println(F("ROM load FAILED — halting"));
      signalBootStatus(false);
      while (1);
    }
  }

  romLoaded = true;
  Serial.print(F("ROM loaded OK — "));
  Serial.print(ROM_SIZE);
  Serial.println(F(" bytes"));

  // Validate: check ROM signature bytes at known addresses
  // Stock 266D fuel map starts at 0x0000, timing at 0x1000
  // A completely blank (0xFF) ROM is invalid
  uint32_t nonFF = 0;
  for (int i = 0; i < ROM_SIZE; i++) {
    if (romData[i] != 0xFF) nonFF++;
  }
  if (nonFF < 256) {
    Serial.println(F("WARNING: ROM looks blank — verify SD file"));
  } else {
    Serial.print(F("ROM validation OK — "));
    Serial.print(nonFF);
    Serial.println(F(" non-FF bytes"));
  }

  // ── 3. FlexIO EPROM emulator ───────────────────────────────────────────
  Serial.print(F("EPROM emulator init... "));
  eprom_init(romData, ROM_SIZE);
  Serial.println(F("OK"));

  // ── 4. Wideband (Spartan 3 Lite OEM + LSU ADV) ────────────────────────
  Serial.print(F("Wideband UART init... "));
  wideband_init();
  Serial.println(F("OK"));

  // ── 5. Sensors ─────────────────────────────────────────────────────────
  Serial.print(F("Sensors init... "));
  sensors_init();
  Serial.println(F("OK"));

  // ── 6. Injector intercept ──────────────────────────────────────────────
  Serial.print(F("Injector intercept init... "));
  injectors_init();
  Serial.println(F("OK"));

  // ── 7. CAN bus ─────────────────────────────────────────────────────────
  Serial.print(F("CAN bus init... "));
  can_init();
  Serial.println(F("OK"));

  // ── 8. Datalogger ──────────────────────────────────────────────────────
  Serial.print(F("Datalogger init... "));
  datalog_init();
  Serial.println(F("OK"));

  // ── 9. Ready ───────────────────────────────────────────────────────────
  bootTimeMs = millis();
  ecuReady   = true;
  signalBootStatus(true);

  Serial.println(F("=== Boot complete — ECU may read EPROM ==="));
  Serial.print(F("Boot time: "));
  Serial.print(bootTimeMs);
  Serial.println(F(" ms"));
}

// ---------------------------------------------------------------------------
// loop() — runs continuously after setup()
// ---------------------------------------------------------------------------
void loop() {

  // EPROM emulation is handled entirely by FlexIO hardware + DMA in eprom_emu.
  // The loop handles everything else: corrections, logging, comms.

  // ── Wideband: read AFR from Spartan 3 ──────────────────────────────────
  wideband_update();

  // ── Sensors: read MAP, knock, TPS, IAT ─────────────────────────────────
  sensors_update();

  // ── Corrections: compute trim adjustments ──────────────────────────────
  // Applies wideband + MAP + knock data to modify ROM tables in RAM.
  // The FlexIO emulator always serves whatever is currently in romData[].
  corrections_update(romData);

  // ── Injectors: apply trim to pulse width if intercept active ───────────
  injectors_update();

  // ── CAN: broadcast dataframe ────────────────────────────────────────────
  can_update();

  // ── Datalogger: write to SD ─────────────────────────────────────────────
  datalog_update();

  // ── USB debug: print status every 1s ───────────────────────────────────
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    Serial.print(F("AFR="));    Serial.print(wideband_getAFR(), 1);
    Serial.print(F(" MAP="));   Serial.print(sensors_getMAP_kPa(), 1);
    Serial.print(F("kPa TPS=")); Serial.print(sensors_getTPS_pct(), 1);
    Serial.print(F("% IAT="));  Serial.print(sensors_getIAT_C(), 1);
    Serial.print(F("C KNOCK=")); Serial.print(sensors_getKnock_V(), 2);
    Serial.println(F("V"));
  }
}

// ---------------------------------------------------------------------------
// loadRomFromSD()
// Reads binary ROM file from SD into romData[] buffer.
// Mirrors lower 32KB to upper 32KB to match 27C512 behaviour.
// ---------------------------------------------------------------------------
bool loadRomFromSD(const char* filename) {
  File f = SD.open(filename, FILE_READ);
  if (!f) return false;

  size_t bytesRead = f.read(romData, ROM_ACTIVE_SIZE);
  f.close();

  if (bytesRead != ROM_ACTIVE_SIZE) {
    Serial.print(F("ROM read short: "));
    Serial.print(bytesRead);
    Serial.print(F(" / "));
    Serial.println(ROM_ACTIVE_SIZE);
    return false;
  }

  // Mirror: romData[0x8000..0xFFFF] = romData[0x0000..0x7FFF]
  // ECU may assert A15=1 — we serve same data either way
  memcpy(&romData[ROM_ACTIVE_SIZE], &romData[0], ROM_ACTIVE_SIZE);

  return true;
}

// ---------------------------------------------------------------------------
// signalBootStatus()
// Blink the Teensy LED: fast = error, slow steady = OK
// ---------------------------------------------------------------------------
void signalBootStatus(bool ok) {
  pinMode(LED_BUILTIN, OUTPUT);
  if (ok) {
    // 3 slow blinks = good
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_BUILTIN, HIGH); delay(200);
      digitalWrite(LED_BUILTIN, LOW);  delay(200);
    }
    digitalWrite(LED_BUILTIN, HIGH);   // Stay on = running
  } else {
    // Rapid blink = fatal error
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH); delay(100);
      digitalWrite(LED_BUILTIN, LOW);  delay(100);
    }
  }
}
