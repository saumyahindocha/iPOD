// Pod: DAC tone test
// Plays a steady 440 Hz sine tone through the PCM5102 I2S DAC.
// Wiring: DIN -> GP9, BCK -> GP10, WSEL (LRCK) -> GP11.
// The onboard LED stays on while the tone plays; fast blinking means I2S failed to start.

#include <I2S.h>

I2S i2s(OUTPUT);

const int sampleRate = 44100;
const float freq = 440.0;      // A4 note
const int16_t volume = 3000;   // kept low on purpose; max is 32767
float phase = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  i2s.setBCLK(10);             // BCK on GP10, so WSEL is automatically GP11
  i2s.setDATA(9);              // DIN on GP9
  i2s.setBitsPerSample(16);

  if (!i2s.begin(sampleRate)) {
    while (true) {             // fast blink = I2S failed to start
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(100);
    }
  }
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  int16_t sample = (int16_t)(volume * sinf(phase));
  i2s.write(sample);           // left channel
  i2s.write(sample);           // right channel
  phase += 2.0f * PI * freq / sampleRate;
  if (phase > 2.0f * PI) phase -= 2.0f * PI;
}
