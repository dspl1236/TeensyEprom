#pragma once
#include <Arduino.h>
// wideband.h — Spartan 3 Lite OEM UART wideband interface

void  wideband_init();
void  wideband_update();
float wideband_getAFR();
int   wideband_getTemp_C();
bool  wideband_isValid();
