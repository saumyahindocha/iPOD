#include "pod_battery.h"
#include "pod_pins.h"
#include "pod_display.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define MAX17048_ADDR   0x36
#define REG_VCELL       0x02   // 78.125 uV / LSB
#define REG_SOC         0x04   // high byte = %, low byte = 1/256 %
#define REG_VERSION     0x08
#define REG_CRATE       0x16   // signed, 0.208 %/hour

static bool present;
static int percent = -1;
static uint32_t millivolts;
static bool charging;
static bool warned;

static bool read_reg(uint8_t reg, uint16_t *val) {
    uint8_t b[2];
    absolute_time_t t = make_timeout_time_ms(5);
    if (i2c_write_blocking_until(POD_I2C, MAX17048_ADDR, &reg, 1, true, t) != 1) return false;
    if (i2c_read_blocking_until(POD_I2C, MAX17048_ADDR, b, 2, false, make_timeout_time_ms(5)) != 2) return false;
    *val = (uint16_t)((b[0] << 8) | b[1]);
    return true;
}

void pod_battery_init(void) {
    i2c_init(POD_I2C, 100 * 1000);
    gpio_set_function(POD_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(POD_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(POD_I2C_SDA_PIN);
    gpio_pull_up(POD_I2C_SCL_PIN);
#if POD_CHG_STAT_PIN >= 0
    gpio_init(POD_CHG_STAT_PIN);
    gpio_set_dir(POD_CHG_STAT_PIN, GPIO_IN);
    gpio_pull_up(POD_CHG_STAT_PIN);
#endif
    uint16_t v;
    present = read_reg(REG_VERSION, &v);
    printf("Pod: fuel gauge %s\n", present ? "found (MAX17048)" : "not fitted - battery icon hidden");
    pod_battery_poll();
}

bool pod_battery_present(void)  { return present; }
int  pod_battery_percent(void)  { return percent; }
uint32_t pod_battery_mv(void)   { return millivolts; }
bool pod_battery_charging(void) { return charging; }

void pod_battery_poll(void) {
    if (!present) { pod_display_set_battery(-1, false); return; }
    uint16_t soc, vcell, crate;
    if (!read_reg(REG_SOC, &soc) || !read_reg(REG_VCELL, &vcell)) return;
    percent = soc >> 8;
    if (percent > 100) percent = 100;
    millivolts = (uint32_t)vcell * 78125u / 1000000u;
#if POD_CHG_STAT_PIN >= 0
    charging = !gpio_get(POD_CHG_STAT_PIN);
#else
    charging = read_reg(REG_CRATE, &crate) && (int16_t)crate > 25;   // > ~5 %/h and rising
#endif
    pod_display_set_battery(percent, charging);

    // Low-battery warning: once per discharge, re-armed by charging or a recovery above 17 %
    if (!warned && !charging && percent < POD_LOW_BATTERY_PCT) {
        char msg[32];
        snprintf(msg, sizeof msg, "Battery low \xC2\xB7 %d%%", percent);
        pod_display_toast(msg);
        warned = true;
    } else if (warned && (charging || percent >= POD_LOW_BATTERY_PCT + 2)) {
        warned = false;
    }
}
