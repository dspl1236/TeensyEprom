#pragma once
// sensors.h — MAP, TPS, IAT, knock, RPM sensor readings

void  sensors_init();
void  sensors_update();
float sensors_getMAP_kPa();
float sensors_getTPS_pct();
float sensors_getIAT_C();
float sensors_getKnock_V();
bool  sensors_isKnocking();
int   sensors_getRPM();
