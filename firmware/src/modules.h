// =============================================================================
// wideband.h — Spartan 3 Lite OEM UART interface
// Parses AFR stream: "0:a:14.7\n1:a:780\n2:a:3000\n3:a:129\n"
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

void  wideband_init();
void  wideband_update();          // Call every loop()
float wideband_getAFR();          // Returns last valid AFR
int   wideband_getTemp_C();       // LSU sensor temp in °C
bool  wideband_isValid();         // False if no data in last 2s

// =============================================================================
// sensors.h — MAP, knock, TPS, IAT
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

void  sensors_init();
void  sensors_update();

float sensors_getMAP_kPa();       // Manifold absolute pressure
float sensors_getTPS_pct();       // Throttle position 0–100%
float sensors_getIAT_C();         // Intake air temp °C
float sensors_getKnock_V();       // Knock sensor AC amplitude (volts above bias)
bool  sensors_isKnocking();       // True if knock threshold exceeded
int   sensors_getRPM();           // Engine RPM from coil trigger

// =============================================================================
// injectors.h — MOSFET injector pulse width intercept
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

void  injectors_init();
void  injectors_update();
void  injectors_setTrim(float trimFraction); // -0.15 to +0.15 (±15%)

// =============================================================================
// can_bus.h — TJA1051 CAN transceivers
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

void can_init();
void can_update();               // Broadcast sensor frame

// =============================================================================
// datalog.h — SD card CSV logger
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

void datalog_init();
void datalog_update();           // Write row at DATALOG_INTERVAL_MS rate

// =============================================================================
// corrections.h — Closed-loop fuel + knock retard
// Modifies romData[] in RAM. FlexIO emulator always serves current romData.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

void corrections_init();
void corrections_update(uint8_t* romData);

// Current correction state (read-only from other modules)
float corrections_getFuelTrim();     // Current fuel trim fraction
float corrections_getKnockRetard();  // Current knock retard degrees
