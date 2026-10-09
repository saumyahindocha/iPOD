#include "pod_battery.h"
#include "pod_pins.h"
#include "pod_display.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"

#define MAX17048_ADDR   0x36
#define REG_VCELL       0x02   // 78.125 uV / LSB
#define REG_SOC         0x04   // high byte = %, low byte = 1/256 %
#define REG_VERSION     0x08
#define REG_CRATE       0x16   // signed, 0.208 %/hour

static bool present;          // any battery reading available
static bool use_adc;          // true: GP28 divider, false: MAX17048
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

// ---- GP28 divider (perfboard) -------------------------------------------
static uint32_t adc_node_mv(void) {           // average of 16 samples, mV at the pin
    adc_select_input(POD_VBAT_ADC_PIN - 26);
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) sum += adc_read();
    return (sum / 16) * 3300u / 4095u;
}

// Resting LiPo voltage -> percent (rough, but good enough for an icon)
static int mv_to_percent(uint32_t mv) {
    static const uint16_t v[] = {3300, 3500, 3600, 3650, 3700, 3750, 3800, 3900, 4000, 4100, 4180};
    static const uint8_t  p[] = {   0,    5,   10,   20,   30,   40,   50,   65,   80,   92,  100};
    if (mv <= v[0]) return 0;
    for (int i = 1; i < 11; i++)
        if (mv < v[i]) return p[i-1] + (int)((mv - v[i-1]) * (p[i] - p[i-1]) / (v[i] - v[i-1]));
    return 100;
}

static bool adc_probe(void) {
    adc_init();
    adc_gpio_init(POD_VBAT_ADC_PIN);
    gpio_pull_down(POD_VBAT_ADC_PIN);         // a floating pin reads ~0 with the pull-down
    sleep_ms(2);
    uint32_t node = adc_node_mv();
    gpio_disable_pulls(POD_VBAT_ADC_PIN);
    return node > 400;                        // divider fitted (100k-150k): >0.6 V even with the pull-down; floating reads ~0
}

static uint32_t mv_filtered;                   // smoothed battery mV
static uint32_t mv_trend_ref;                  // value one minute ago, for "charging"
static absolute_time_t trend_next;

static void adc_poll(void) {
    uint32_t mv = adc_node_mv() * POD_VBAT_DIVIDER;
    mv_filtered = mv_filtered ? (mv_filtered * 7 + mv) / 8 : mv;
    millivolts = mv_filtered;
    percent = mv_to_percent(mv_filtered);
    if (absolute_time_diff_us(trend_next, get_absolute_time()) >= 0) {
        // Charging pushes the voltage up; playback only ever pulls it down.
        charging = mv_trend_ref && mv_filtered > mv_trend_ref + 15;
        mv_trend_ref = mv_filtered;
        trend_next = make_timeout_time_ms(60 * 1000);
    }
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
    if (!present && adc_probe()) { present = true; use_adc = true; }
    printf("Pod: battery %s\n", !present ? "not measured - battery icon hidden"
                                         : use_adc ? "on GP28 divider" : "gauge found (MAX17048)");
    pod_battery_poll();
}

bool pod_battery_present(void)  { return present; }
int  pod_battery_percent(void)  { return percent; }
uint32_t pod_battery_mv(void)   { return millivolts; }
bool pod_battery_charging(void) { return charging; }

void pod_battery_poll(void) {
    if (!present) { pod_display_set_battery(-1, false); return; }
    uint16_t soc, vcell, crate;
    if (use_adc) {
        adc_poll();
        pod_display_set_battery(percent, charging);
        goto warn;
    }
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

warn:
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
