// Pod: Bluetooth audio receiver (A2DP sink)
// Pair your phone with "Pod", play music, and it comes out of the PCM5102 DAC.
// Wiring: DIN -> GP9, BCK -> GP10, WSEL (LRCK) -> GP11.
//
// Onboard LED: slow blink = waiting for a phone, steady on = connected.
// BOOTSEL button: pause / resume.
// Track title and artist are printed to the Serial Monitor (115200 baud).

#include <BluetoothAudio.h>
#include <I2S.h>

I2S i2s(OUTPUT);
A2DPSink a2dp;

volatile bool connected = false;
volatile A2DPSink::PlaybackStatus status = A2DPSink::STOPPED;

void connectCB(void *param, bool isConnected) {
  (void) param;
  connected = isConnected;
  Serial.printf(isConnected ? "Phone connected\n" : "Phone disconnected\n");
}

void playbackCB(void *param, A2DPSink::PlaybackStatus state) {
  (void) param;
  status = state;
}

void volumeCB(void *param, int pct) {
  (void) param;
  Serial.printf("Volume: %d%%\n", pct);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  i2s.setBCLK(10);   // BCK on GP10, so WSEL is automatically GP11
  i2s.setDATA(9);    // DIN on GP9
  i2s.setBitsPerSample(16);

  a2dp.setName("Pod");
  a2dp.setConsumer(new BluetoothAudioConsumerI2S(i2s));
  a2dp.onConnect(connectCB);
  a2dp.onPlaybackStatus(playbackCB);
  a2dp.onVolume(volumeCB);
  a2dp.begin();

  Serial.printf("Pod is ready. Pair your phone with \"Pod\".\n");
}

String lastTitle;
unsigned long lastBlink = 0;

void loop() {
  // LED: steady when connected, slow blink while waiting
  if (connected) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (millis() - lastBlink > 500) {
    lastBlink = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  // BOOTSEL = pause / resume
  if (BOOTSEL) {
    if (status == A2DPSink::PLAYING) {
      a2dp.pause();
    } else if (status == A2DPSink::PAUSED) {
      a2dp.play();
    }
    while (BOOTSEL) {
      delay(10);
    }
  }

  // Print the track whenever it changes
  const char *title = a2dp.trackTitle();
  if (title && lastTitle != title) {
    lastTitle = title;
    Serial.printf("Now playing: %s - %s\n", a2dp.trackTitle(), a2dp.trackArtist());
  }
}
