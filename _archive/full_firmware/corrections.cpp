// =============================================================================
// corrections.cpp — Closed-loop fuel trim + knock retard
//
// This module modifies romData[] in RAM. The FlexIO EPROM emulator always
// serves whatever is currently in romData[], so corrections take effect
// immediately on the next ECU read cycle — no ECU reset needed.
//
// Fuel trim:
//   Compares wideband AFR to target AFR, adjusts fuel map cells in RAM
//   for the current MAP/RPM operating point.
//   Limit: ±FUEL_TRIM_MAX (default ±10%)
//
// Knock retard:
//   On knock event, retards timing map cells at current operating point.
//   Recovers gradually when knock clears.
//   Limit: KNOCK_RETARD_MAX (default 10°)
//
// Operating point:
//   RPM axis:  0–6000 RPM in 16 steps (375 RPM/step) — matches 7A fuel map
//   Load axis: 0–100% MAP in 16 steps  (6.25%/step)
// =============================================================================
#include "corrections.h"
#include "config.h"
#include "wideband.h"
#include "sensors.h"

// ---------------------------------------------------------------------------
// Known ROM map addresses (893906266D, confirmed from ROM analysis)
// ---------------------------------------------------------------------------
#define FUEL_MAP_BASE    0x0000    // 16×16 fuel map
#define TIMING_MAP_BASE  0x1000    // 16×16 timing map

// Map dimensions
#define MAP_ROWS         16
#define MAP_COLS         16

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static float fuelTrim      = 0.0f;   // Current trim fraction (-0.10 to +0.10)
static float knockRetard   = 0.0f;   // Current knock retard degrees

// Shadow copy of original ROM values for the cells we've modified
// Allows us to apply trim relative to original, not accumulate errors
static uint8_t origFuelMap[MAP_ROWS * MAP_COLS];
static uint8_t origTimingMap[MAP_ROWS * MAP_COLS];
static bool    shadowCaptured = false;

// Update rate limiting
static uint32_t lastFuelUpdate  = 0;
static uint32_t lastKnockUpdate = 0;
#define FUEL_UPDATE_MS    100    // Fuel trim update every 100ms
#define KNOCK_UPDATE_MS    50    // Knock check every 50ms

// ---------------------------------------------------------------------------
// getOperatingPoint() — returns current row/col in 16×16 map
// ---------------------------------------------------------------------------
static void getOperatingPoint(int* row, int* col) {
  int rpm  = sensors_getRPM();
  float map_kpa = sensors_getMAP_kPa();

  // RPM axis: 0–6000 RPM, 16 cells
  *col = constrain((int)(rpm / 375.0f), 0, MAP_COLS - 1);

  // Load axis: ~20kPa (idle vacuum) to ~100kPa (WOT), 16 cells
  // MAP sensor range 10–304 kPa, but naturally aspirated engine: 20–102 kPa
  *row = constrain((int)((map_kpa - 20.0f) / 5.125f), 0, MAP_ROWS - 1);
}

// ---------------------------------------------------------------------------
// corrections_init()
// ---------------------------------------------------------------------------
void corrections_init() {
  fuelTrim    = 0.0f;
  knockRetard = 0.0f;
}

// ---------------------------------------------------------------------------
// corrections_update() — call every loop()
// ---------------------------------------------------------------------------
void corrections_update(uint8_t* romData) {

  // First run: capture shadow copy of original map values
  if (!shadowCaptured) {
    memcpy(origFuelMap,   &romData[FUEL_MAP_BASE],   MAP_ROWS * MAP_COLS);
    memcpy(origTimingMap, &romData[TIMING_MAP_BASE],  MAP_ROWS * MAP_COLS);
    shadowCaptured = true;
    Serial.println(F("Corrections: shadow maps captured"));
  }

  int row, col;
  getOperatingPoint(&row, &col);
  int mapIdx = row * MAP_COLS + col;

  // ── Fuel trim (closed loop) ─────────────────────────────────────────────
  if (millis() - lastFuelUpdate >= FUEL_UPDATE_MS) {
    lastFuelUpdate = millis();

    if (wideband_isValid()) {
      float afr = wideband_getAFR();

      // Simple proportional correction — step toward target
      if (afr > AFR_TARGET + 0.1f) {
        // Lean — add fuel (increase map value)
        fuelTrim += FUEL_TRIM_STEP;
      } else if (afr < AFR_TARGET - 0.1f) {
        // Rich — remove fuel (decrease map value)
        fuelTrim -= FUEL_TRIM_STEP;
      }

      // Clamp trim
      fuelTrim = constrain(fuelTrim, -FUEL_TRIM_MAX, FUEL_TRIM_MAX);

      // Apply trim to current operating cell only
      // Trim is relative to original captured shadow value
      int origVal = origFuelMap[mapIdx];
      int newVal  = (int)(origVal * (1.0f + fuelTrim));
      newVal = constrain(newVal, 0, 255);
      romData[FUEL_MAP_BASE + mapIdx] = (uint8_t)newVal;
      // Mirror to upper half
      romData[0x8000 + FUEL_MAP_BASE + mapIdx] = (uint8_t)newVal;
    }
    // If wideband invalid: hold last trim, don't adjust
  }

  // ── Knock retard ────────────────────────────────────────────────────────
  if (millis() - lastKnockUpdate >= KNOCK_UPDATE_MS) {
    lastKnockUpdate = millis();

    if (sensors_isKnocking()) {
      // Knock detected — retard immediately
      knockRetard += KNOCK_RETARD_DEG;
      knockRetard  = min(knockRetard, KNOCK_RETARD_MAX);

      Serial.print(F("KNOCK! Retard now: "));
      Serial.print(knockRetard, 1);
      Serial.println(F("°"));
    } else {
      // No knock — recover gradually
      knockRetard -= KNOCK_RECOVER_DEG;
      knockRetard  = max(knockRetard, 0.0f);
    }

    if (knockRetard > 0.0f) {
      // Apply retard to timing map at current operating point
      // Timing map values: each count ≈ 0.75° (varies by ECU — TODO: calibrate)
      // Conservative: assume 1 count = 1° for now
      int origTiming = origTimingMap[mapIdx];
      int newTiming  = origTiming - (int)knockRetard;
      newTiming = constrain(newTiming, 0, 255);
      romData[TIMING_MAP_BASE + mapIdx] = (uint8_t)newTiming;
      romData[0x8000 + TIMING_MAP_BASE + mapIdx] = (uint8_t)newTiming;
    } else {
      // Restore original timing for this cell
      romData[TIMING_MAP_BASE + mapIdx] = origTimingMap[mapIdx];
      romData[0x8000 + TIMING_MAP_BASE + mapIdx] = origTimingMap[mapIdx];
    }
  }
}

// ---------------------------------------------------------------------------
float corrections_getFuelTrim()    { return fuelTrim; }
float corrections_getKnockRetard() { return knockRetard; }
