// =============================================================================
// maf.h — MAF sensor intercept + signal translation
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

// MAF input type — set in config.h
#define MAF_INPUT_FREQUENCY  0    // Phase 1: stock 7A hotwire (frequency)
#define MAF_INPUT_ANALOG     1    // Phase 2: MK4 1.8T hot film (0–5V analog)

void  maf_init();
void  maf_update();

float maf_getAirflow_kgh();     // Current measured airflow kg/h
float maf_getOutputFreq_Hz();   // Current output frequency to ECU (Hz)
float maf_getInputVoltage();    // Phase 2 only: 1.8T sensor voltage
