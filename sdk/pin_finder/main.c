// pod_pin_finder - identifies the three DAC signal pins with a multimeter.
// Each pin blinks at its own slow rate so the meter can follow it:
//   DIN  (GP13) : 1 blink per second   (3.3 V for 0.5 s, 0 V for 0.5 s)
//   BCK  (GP14) : on 0.25 s, off 0.25 s
//   WSEL (GP15) : on 2 s, off 2 s
// Put the meter on the DAC's pads and you can see which signal reaches which pad.
#include "pico/stdlib.h"
#include "pod_pins.h"

int main(void) {
    const uint pins[3] = { POD_I2S_DIN_PIN, POD_I2S_BCK_PIN, POD_I2S_LRCK_PIN };
    const uint32_t half_ms[3] = { 500, 250, 2000 };
    for (int i = 0; i < 3; i++) { gpio_init(pins[i]); gpio_set_dir(pins[i], GPIO_OUT); }
    uint32_t t = 0;
    while (true) {
        for (int i = 0; i < 3; i++) gpio_put(pins[i], (t / half_ms[i]) & 1);
        sleep_ms(10);
        t += 10;
    }
}
