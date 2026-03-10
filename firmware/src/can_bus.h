#pragma once
// can_bus.h — CAN bus output via FlexCAN_T4
// (init and update are declared in datalog.h, this header
//  exists for modules that include can_bus.h directly)

void can_init();
void can_update();
