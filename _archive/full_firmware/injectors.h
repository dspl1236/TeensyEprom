#pragma once
#include <Arduino.h>
// injectors.h — MOSFET injector intercept for pulse-width correction

void  injectors_init();
void  injectors_update();
void  injectors_setTrim(float trim);
