// pod_battery - battery percentage from a MAX17048 fuel gauge (I2C) or, on the
// perfboard, a 100k/100k divider on GP28; plus the low-battery warning. Safe to call when no gauge is fitted: everything reports
// "unknown" and the on-screen icon stays hidden.
#pragma once
#include <stdbool.h>
#include <stdint.h>

void pod_battery_init(void);        // probe the gauge (core 0)
bool pod_battery_present(void);
int  pod_battery_percent(void);     // 0..100, or -1 if unknown
uint32_t pod_battery_mv(void);      // cell voltage in mV, 0 if unknown
bool pod_battery_charging(void);

// Call every few seconds from core 0. Reads the gauge, updates the status-bar
// icon, and shows "Battery low" once when it drops below POD_LOW_BATTERY_PCT.
void pod_battery_poll(void);
