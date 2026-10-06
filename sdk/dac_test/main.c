// pod_dac_test - PCM5102 bring-up with NO Bluetooth involved.
//
// Plays a quiet 440 Hz tone in a repeating 3-step pattern:
//   LEFT only (2 s) -> RIGHT only (2 s) -> BOTH (2 s) -> 1 s silence
// so you can confirm the I2S wiring AND the channel order by ear.
// Amplitude is ~-20 dBFS (10% of full scale) on purpose - safe for a first
// listen. Status is printed over USB serial (115200, any serial monitor).
//
// Wiring: see common/pod_pins.h  (DIN=GP9, BCK=GP10, LRCK/WSEL=GP11)

#include <math.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pod_pins.h"

#define SAMPLE_RATE       44100
#define SAMPLES_PER_BUF   256
#define TONE_HZ           440.0f
#define AMPLITUDE         3277      // 10% of 32767

static audio_buffer_pool_t *init_i2s(void) {
    static audio_format_t fmt = {
        .sample_freq = SAMPLE_RATE,
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = 2,
    };
    static audio_buffer_format_t producer_fmt = { .format = &fmt, .sample_stride = 4 };

    audio_buffer_pool_t *pool = audio_new_producer_pool(&producer_fmt, 3, SAMPLES_PER_BUF);

    audio_i2s_config_t cfg = {
        .data_pin = POD_I2S_DIN_PIN,
        .clock_pin_base = POD_I2S_BCK_PIN,
        .dma_channel = 0,
        .pio_sm = 0,
    };
    const audio_format_t *out = audio_i2s_setup(&fmt, &cfg);
    if (!out) panic("audio_i2s_setup failed");
    if (!audio_i2s_connect(pool)) panic("audio_i2s_connect failed");
    audio_i2s_set_enabled(true);
    return pool;
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500);  // give a serial monitor time to attach
    printf("\n[pod_dac_test] I2S: DIN=GP%d BCK=GP%d LRCK=GP%d, %d Hz\n",
           POD_I2S_DIN_PIN, POD_I2S_BCK_PIN, POD_I2S_LRCK_PIN, SAMPLE_RATE);

    audio_buffer_pool_t *pool = init_i2s();

    // one-cycle-accurate phase accumulator
    float phase = 0.0f;
    const float step = 2.0f * (float)M_PI * TONE_HZ / SAMPLE_RATE;

    const char *names[] = {"LEFT only", "RIGHT only", "BOTH channels", "silence"};
    const uint32_t durations_ms[] = {2000, 2000, 2000, 1000};
    int stage = 0;
    absolute_time_t stage_end = make_timeout_time_ms(durations_ms[0]);
    printf("[pod_dac_test] %s\n", names[stage]);

    while (true) {
        if (time_reached(stage_end)) {
            stage = (stage + 1) % 4;
            stage_end = make_timeout_time_ms(durations_ms[stage]);
            printf("[pod_dac_test] %s\n", names[stage]);
        }
        audio_buffer_t *buf = take_audio_buffer(pool, true);
        int16_t *s = (int16_t *)buf->buffer->bytes;
        for (uint i = 0; i < buf->max_sample_count; i++) {
            int16_t v = (int16_t)(AMPLITUDE * sinf(phase));
            phase += step;
            if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
            s[2 * i]     = (stage == 0 || stage == 2) ? v : 0;  // left
            s[2 * i + 1] = (stage == 1 || stage == 2) ? v : 0;  // right
        }
        buf->sample_count = buf->max_sample_count;
        give_audio_buffer(pool, buf);
    }
}
