// Pod: Now Playing (Bluetooth + screen + touch)
// Streams audio from the phone to the DAC and shows the track on the screen,
// with touch buttons for previous / play-pause / next.
//
// Wiring: see README. DAC on GP9/10/11, display + touch on SPI0 (GP18/19/20).

#include <BluetoothAudio.h>
#include <I2S.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

// ---------- Pins ----------
const int PIN_SCK = 18, PIN_MOSI = 19, PIN_MISO = 20;
const int TFT_CS = 17, TFT_DC = 16, TFT_RST = 21, TFT_LED = 13;
const int TOUCH_CS = 22, TOUCH_IRQ = 26;
const int I2S_DATA = 9, I2S_BCLK = 10;   // WSEL is GP11 (BCLK + 1)

// ---------- Touch calibration (measured 2026-10-07; both axes reversed) ----------
const int RAW_X_LEFT = 3540, RAW_X_RIGHT = 565;
const int RAW_Y_TOP  = 3680, RAW_Y_BOTTOM = 380;

// ---------- Colours (RGB565) ----------
const uint16_t BG      = 0x0000;   // black
const uint16_t ACCENT  = 0xF9A6;   // warm coral; later this will come from the album art
const uint16_t TEXT    = 0xFFFF;
const uint16_t SUBTEXT = 0xAD55;   // light grey
const uint16_t DIM     = 0x4208;   // dark grey

I2S i2s(OUTPUT);
A2DPSink a2dp;
Adafruit_ILI9341 tft(&SPI, TFT_DC, TFT_CS, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// State shared with Bluetooth callbacks
volatile bool connected = false;
volatile bool trackChanged = true;
volatile bool statusChanged = true;
volatile A2DPSink::PlaybackStatus status = A2DPSink::STOPPED;

void connectCB(void *, bool c) { connected = c; trackChanged = true; statusChanged = true; }
void playbackCB(void *, A2DPSink::PlaybackStatus s) { status = s; statusChanged = true; }
void trackCB(void *) { trackChanged = true; }

// ---------- Layout (portrait 240 x 320) ----------
const int ART_Y = 0, ART_H = 150;        // placeholder area where album art will go
const int TITLE_Y = 166, ARTIST_Y = 196, ALBUM_Y = 218;
const int BTN_Y = 262, BTN_R = 26;       // control row centre and size
const int BTN_PREV_X = 50, BTN_PLAY_X = 120, BTN_NEXT_X = 190;

// Print a string centred on a line, cutting it short with ".." if it is too wide
void centred(const char *s, int y, uint8_t size, uint16_t colour) {
  tft.setTextSize(size);
  tft.setTextColor(colour);
  int maxChars = 240 / (6 * size);
  char buf[48];
  strncpy(buf, (s && *s) ? s : "-", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  if ((int)strlen(buf) > maxChars) {
    buf[maxChars - 2] = '.'; buf[maxChars - 1] = '.'; buf[maxChars] = 0;
  }
  int w = strlen(buf) * 6 * size;
  tft.setCursor((240 - w) / 2, y);
  tft.print(buf);
}

void drawArtPlaceholder() {
  // Vertical fade from the accent colour into black: the "Poster" look, minus the art
  for (int y = 0; y < ART_H; y++) {
    uint8_t r = ((ACCENT >> 11) & 0x1F) * (ART_H - y) / ART_H;
    uint8_t g = ((ACCENT >> 5) & 0x3F) * (ART_H - y) / ART_H;
    uint8_t b = (ACCENT & 0x1F) * (ART_H - y) / ART_H;
    tft.drawFastHLine(0, ART_Y + y, 240, (r << 11) | (g << 5) | b);
  }
  tft.setTextSize(6);
  tft.setTextColor(TEXT);
  tft.setCursor(120 - 18, 45);
  tft.print((char)14);   // music note glyph
}

void drawTrack() {
  tft.fillRect(0, ART_H, 240, BTN_Y - BTN_R - 8 - ART_H, BG);
  if (!connected) {
    centred("Pod", TITLE_Y, 3, TEXT);
    centred("Pair your phone with \"Pod\"", ARTIST_Y + 8, 1, SUBTEXT);
    return;
  }
  centred(a2dp.trackTitle(), TITLE_Y, 2, TEXT);
  centred(a2dp.trackArtist(), ARTIST_Y, 2, ACCENT);
  centred(a2dp.trackAlbum(), ALBUM_Y, 1, SUBTEXT);
}

void drawControls() {
  uint16_t c = connected ? TEXT : DIM;
  tft.fillRect(0, BTN_Y - BTN_R - 2, 240, 2 * BTN_R + 4, BG);

  // Previous: |<<
  tft.fillRect(BTN_PREV_X - 14, BTN_Y - 10, 3, 20, c);
  tft.fillTriangle(BTN_PREV_X - 10, BTN_Y, BTN_PREV_X, BTN_Y - 10, BTN_PREV_X, BTN_Y + 10, c);
  tft.fillTriangle(BTN_PREV_X, BTN_Y, BTN_PREV_X + 10, BTN_Y - 10, BTN_PREV_X + 10, BTN_Y + 10, c);

  // Play / pause in a filled circle
  tft.fillCircle(BTN_PLAY_X, BTN_Y, BTN_R, connected ? ACCENT : DIM);
  if (status == A2DPSink::PLAYING) {
    tft.fillRect(BTN_PLAY_X - 9, BTN_Y - 11, 6, 22, BG);
    tft.fillRect(BTN_PLAY_X + 3, BTN_Y - 11, 6, 22, BG);
  } else {
    tft.fillTriangle(BTN_PLAY_X - 7, BTN_Y - 12, BTN_PLAY_X - 7, BTN_Y + 12, BTN_PLAY_X + 12, BTN_Y, BG);
  }

  // Next: >>|
  tft.fillTriangle(BTN_NEXT_X - 10, BTN_Y - 10, BTN_NEXT_X - 10, BTN_Y + 10, BTN_NEXT_X, BTN_Y, c);
  tft.fillTriangle(BTN_NEXT_X, BTN_Y - 10, BTN_NEXT_X, BTN_Y + 10, BTN_NEXT_X + 10, BTN_Y, c);
  tft.fillRect(BTN_NEXT_X + 11, BTN_Y - 10, 3, 20, c);
}

void setup() {
  Serial.begin(115200);

  // Screen + touch on the shared SPI0 bus
  SPI.setSCK(PIN_SCK);
  SPI.setTX(PIN_MOSI);
  SPI.setRX(PIN_MISO);
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  tft.begin();
  tft.setRotation(0);
  tft.setTextWrap(false);
  tft.fillScreen(BG);
  touch.begin(SPI);
  touch.setRotation(0);

  drawArtPlaceholder();

  // Bluetooth audio into the DAC
  i2s.setBCLK(I2S_BCLK);
  i2s.setDATA(I2S_DATA);
  i2s.setBitsPerSample(16);
  a2dp.setName("Pod");
  a2dp.setConsumer(new BluetoothAudioConsumerI2S(i2s));
  a2dp.onConnect(connectCB);
  a2dp.onPlaybackStatus(playbackCB);
  a2dp.onTrackChanged(trackCB);
  a2dp.begin();
}

String shownTitle;
bool wasTouched = false;
unsigned long lastPoll = 0;

void handleTap(int x, int y) {
  if (!connected || abs(y - BTN_Y) > BTN_R + 10) return;
  if (abs(x - BTN_PREV_X) < 32) {
    a2dp.backward();
  } else if (abs(x - BTN_PLAY_X) < 36) {
    if (status == A2DPSink::PLAYING) a2dp.pause(); else a2dp.play();
  } else if (abs(x - BTN_NEXT_X) < 32) {
    a2dp.forward();
  }
}

void loop() {
  // Redraw only what changed; drawing is slow compared with audio
  const char *t = a2dp.trackTitle();
  if (t && shownTitle != t) { shownTitle = t; trackChanged = true; }
  if (trackChanged)  { trackChanged = false;  drawTrack(); }
  if (statusChanged) { statusChanged = false; drawControls(); }

  // Touch: act once per press, on the moment the finger lands
  if (millis() - lastPoll < 20) return;
  lastPoll = millis();
  bool isTouched = touch.touched();
  if (isTouched && !wasTouched) {
    TS_Point p = touch.getPoint();
    int x = constrain(map(p.x, RAW_X_LEFT, RAW_X_RIGHT, 0, 239), 0, 239);
    int y = constrain(map(p.y, RAW_Y_TOP, RAW_Y_BOTTOM, 0, 319), 0, 319);
    handleTap(x, y);
  }
  wasTouched = isTouched;

  // BOOTSEL still works as play / pause
  if (BOOTSEL) {
    if (status == A2DPSink::PLAYING) a2dp.pause(); else a2dp.play();
    while (BOOTSEL) delay(10);
  }
}
